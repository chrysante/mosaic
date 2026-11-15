#include "support/draw_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#import <Metal/Metal.h>
#include <imgui.h>

#include "ext/stb_rect_pack.h"

using namespace mosaic;

namespace {

// =============================================================================
// Configuration
// =============================================================================

constexpr int atlas_width = 1024;
constexpr int atlas_height = 1024;
constexpr int atlas_gutter = 1;
constexpr int patch_center_size = 2;

constexpr uint32_t threadgroup_width = 8;
constexpr uint32_t threadgroup_height = 8;

// =============================================================================
// Descriptor cache
// =============================================================================

struct ShadowDescHash {
    size_t operator()(ShadowDesc const& desc) const noexcept {
        size_t seed = 0;
        auto combine = [&seed](float value) {
            seed ^= std::hash<float>{}(value) + 0x9e3779b9 + (seed << 6) +
                    (seed >> 2);
        };
        combine(desc.corner_radius);
        combine(desc.shadow_spread);
        combine(desc.shadow_softness);
        return seed;
    }
};

struct ShadowDescEqual {
    bool operator()(ShadowDesc const& a, ShadowDesc const& b) const noexcept {
        return a.corner_radius == b.corner_radius &&
               a.shadow_spread == b.shadow_spread &&
               a.shadow_softness == b.shadow_softness;
    }
};

// =============================================================================
// Compute shader parameters
// Keep this layout in sync with ShadowParams in the Metal shader.
// =============================================================================

struct ShadowParams {
    float corner_radius;
    float shadow_spread;
    float shadow_softness;

    uint32_t texture_width;
    uint32_t texture_height;

    uint32_t allocation_width;
    uint32_t allocation_height;

    uint32_t atlas_x;
    uint32_t atlas_y;

    uint32_t gutter;
    uint32_t padding;
};

// =============================================================================
// Atlas page
// =============================================================================

struct AtlasPage {
    __strong id<MTLTexture> texture = nil;

    std::vector<stbrp_node> nodes;
    stbrp_context pack_context{};

    explicit AtlasPage(id<MTLDevice> device) {
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:atlas_width
                                        height:atlas_height
                                     mipmapped:NO];
        descriptor.usage =
            MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        descriptor.storageMode = MTLStorageModePrivate;
        texture = [device newTextureWithDescriptor:descriptor];
        if (!texture) return;
        texture.label = @"ImGui Shadow Atlas";
        nodes.resize(atlas_width);
        stbrp_init_target(&pack_context, atlas_width, atlas_height,
                          nodes.data(), static_cast<int>(nodes.size()));
    }

    bool valid() const { return texture != nil; }

    bool allocate(int width, int height, stbrp_rect& rect) {
        rect = {};
        rect.w = width;
        rect.h = height;
        if (!stbrp_pack_rects(&pack_context, &rect, 1)) return false;
        return rect.was_packed != 0;
    }
};

// =============================================================================
// Pending compute job
// =============================================================================

struct PendingShadow {
    AtlasPage* page = nullptr;
    ShadowParams params{};
};

// =============================================================================
// Metal shadow manager
// =============================================================================

struct MetalShadowManager final : ShadowManager {
    MetalShadowManager(id<MTLDevice> device, id<MTLCommandQueue> command_queue)
        : device(device)
        , command_queue(command_queue) {}

    ShadowTextureHandle get_shadow_texture(ShadowDesc const& desc) override;

    void end_frame() override;

    bool valid() const { return device != nil && command_queue != nil; }

    bool ensure_pipeline();

    AtlasPage* create_atlas_page();

    bool allocate(int width, int height, AtlasPage*& page, stbrp_rect& rect);

    ShadowTextureHandle create_shadow(ShadowDesc const& desc);

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;

    id<MTLComputePipelineState> pipeline = nil;

    std::vector<std::unique_ptr<AtlasPage>> atlas_pages;

    std::vector<PendingShadow> pending_shadows;

    std::unordered_map<ShadowDesc, ShadowTextureHandle, ShadowDescHash,
                       ShadowDescEqual>
        cache;
};

// =============================================================================
// Runtime Metal shader
// =============================================================================

bool MetalShadowManager::ensure_pipeline() {
    if (pipeline) return true;

    static constexpr char shader_source[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct ShadowParams {
    float corner_radius;
    float shadow_spread;
    float shadow_softness;

    uint texture_width;
    uint texture_height;

    uint allocation_width;
    uint allocation_height;

    uint atlas_x;
    uint atlas_y;

    uint gutter;
    uint padding;
};

float rounded_rect_sdf(float2 p, float2 half_size, float radius) {
    float2 q = abs(p) - (half_size - radius);
    return length(max(q, 0.0))
         + min(max(q.x, q.y), 0.0)
         - radius;
}

kernel void generate_shadow(
    texture2d<float, access::write> atlas [[texture(0)]],
    constant ShadowParams& params [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]]) 
{
    if (gid.x >= params.allocation_width || gid.y >= params.allocation_height)
        return;
    uint2 atlas_position = uint2(params.atlas_x, params.atlas_y) + gid;
    // Initialize the gutter to transparent white.
    if (gid.x < params.gutter || gid.y < params.gutter ||
        gid.x >= params.gutter + params.texture_width ||
        gid.y >= params.gutter + params.texture_height) {
        atlas.write(float4(1.0, 1.0, 1.0, 0.0), atlas_position);
        return;
    }
    uint2 local_position = gid - params.gutter;
    float2 half_size = float2(params.texture_width, params.texture_height) * 
                       0.5 - float2(params.padding);
    float2 p = float2(local_position) + 0.5 - float2(params.padding) - 
               half_size;
    float distance = rounded_rect_sdf(p, half_size, params.corner_radius);
    float alpha = 1.0 - smoothstep(params.shadow_spread, 
                                   params.shadow_spread + params.shadow_softness, 
                                   distance);
    atlas.write(float4(1.0, 1.0, 1.0, alpha), atlas_position);
}
)METAL";

    NSString* source = [NSString stringWithUTF8String:shader_source];
    NSError* error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:source
                                                  options:nil
                                                    error:&error];
    if (!library) {
        NSLog(@"ShadowManager: Failed to compile Metal shader: %@", error);
        return false;
    }
    id<MTLFunction> function = [library newFunctionWithName:@"generate_shadow"];
    if (!function) {
        NSLog(@"ShadowManager: Missing generate_shadow function");
        return false;
    }
    pipeline = [device newComputePipelineStateWithFunction:function
                                                     error:&error];
    if (!pipeline) {
        NSLog(@"ShadowManager: Failed to create compute pipeline: %@", error);
        return false;
    }
    return true;
}

// =============================================================================
// Atlas allocation
// =============================================================================

AtlasPage* MetalShadowManager::create_atlas_page() {
    auto page = std::make_unique<AtlasPage>(device);
    if (!page->valid()) return nullptr;
    AtlasPage* result = page.get();
    atlas_pages.emplace_back(std::move(page));
    return result;
}

bool MetalShadowManager::allocate(int width, int height,
                                  AtlasPage*& result_page,
                                  stbrp_rect& result_rect) {
    for (auto& page : atlas_pages) {
        if (page->allocate(width, height, result_rect)) {
            result_page = page.get();
            return true;
        }
    }
    AtlasPage* page = create_atlas_page();
    if (!page) return false;
    if (!page->allocate(width, height, result_rect)) return false;
    result_page = page;
    return true;
}

// =============================================================================
// Shadow lookup
// =============================================================================

ShadowTextureHandle
MetalShadowManager::get_shadow_texture(ShadowDesc const& desc) {
    if (!valid()) return {};
    if (!std::isfinite(desc.corner_radius) ||
        !std::isfinite(desc.shadow_spread) ||
        !std::isfinite(desc.shadow_softness) || desc.corner_radius < 0.0f ||
        desc.shadow_softness <= 0.0f)
        return {};
    // Return an existing allocation.
    if (auto it = cache.find(desc); it != cache.end()) return it->second;
    // Compile the shader lazily, on the first actual request.
    if (!ensure_pipeline()) return {};
    return create_shadow(desc);
}

// =============================================================================
// Shadow allocation and job creation
// =============================================================================

ShadowTextureHandle MetalShadowManager::create_shadow(ShadowDesc const& desc) {
    int const corner = static_cast<int>(std::ceil(desc.corner_radius));
    int const padding = static_cast<int>(std::ceil(
        std::max(0.0f, desc.shadow_spread + desc.shadow_softness) + 1.0f));
    int const shape_width = corner * 2 + patch_center_size;
    int const shape_height = corner * 2 + patch_center_size;
    int const texture_width = shape_width + padding * 2;
    int const texture_height = shape_height + padding * 2;
    int const allocation_width = texture_width + atlas_gutter * 2;
    int const allocation_height = texture_height + atlas_gutter * 2;
    if (allocation_width > atlas_width || allocation_height > atlas_height)
        return {};
    // Find space in an existing page, or create a new one.
    AtlasPage* page = nullptr;
    stbrp_rect rect{};
    if (!allocate(allocation_width, allocation_height, page, rect)) return {};
    // The actual shadow texture starts after the gutter.
    int const texture_x = rect.x + atlas_gutter;
    int const texture_y = rect.y + atlas_gutter;
    // Schedule GPU generation for end_frame().
    PendingShadow job{};
    job.page = page;
    job.params = {desc.corner_radius,
                  desc.shadow_spread,
                  desc.shadow_softness,
                  static_cast<uint32_t>(texture_width),
                  static_cast<uint32_t>(texture_height),
                  static_cast<uint32_t>(allocation_width),
                  static_cast<uint32_t>(allocation_height),
                  static_cast<uint32_t>(rect.x),
                  static_cast<uint32_t>(rect.y),
                  static_cast<uint32_t>(atlas_gutter),
                  static_cast<uint32_t>(padding)};
    pending_shadows.emplace_back(job);
    // Construct the public handle.
    ShadowTextureHandle handle{};
    // This conversion assumes the ImGui Metal renderer expects
    // an MTLTexture pointer stored in ImTextureID.
    handle.texture_id = (ImTextureID)(intptr_t)(__bridge void*)page->texture;
    handle.uv_min = {static_cast<float>(texture_x) / atlas_width,
                     static_cast<float>(texture_y) / atlas_height};
    handle.uv_max = {
        static_cast<float>(texture_x + texture_width) / atlas_width,
        static_cast<float>(texture_y + texture_height) / atlas_height};

    // Nine-patch boundaries, relative to the inner allocation.
    handle.patch_min = {static_cast<float>(padding + corner),
                        static_cast<float>(padding + corner)};
    handle.patch_max = {
        static_cast<float>(padding + corner + patch_center_size),
        static_cast<float>(padding + corner + patch_center_size)};
    handle.atlas_size = {(float)atlas_width, (float)atlas_height};
    cache.emplace(desc, handle);
    return handle;
}

// =============================================================================
// End of frame: batch and submit all pending shadow generations
// =============================================================================

void MetalShadowManager::end_frame() {
    if (pending_shadows.empty()) return;
    if (!pipeline) return;
    id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
    if (!command_buffer) return;
    command_buffer.label = @"ImGui Shadow Generation";
    id<MTLComputeCommandEncoder> encoder =
        [command_buffer computeCommandEncoder];
    if (!encoder) return;
    encoder.label = @"Shadow SDF Generation";
    [encoder setComputePipelineState:pipeline];
    for (PendingShadow const& job : pending_shadows) {
        [encoder setTexture:job.page->texture atIndex:0];
        [encoder setBytes:&job.params length:sizeof(job.params) atIndex:0];
        NSUInteger const groups_x =
            (job.params.allocation_width + threadgroup_width - 1) /
            threadgroup_width;
        NSUInteger const groups_y =
            (job.params.allocation_height + threadgroup_height - 1) /
            threadgroup_height;
        [encoder dispatchThreadgroups:MTLSizeMake(groups_x, groups_y, 1)
                threadsPerThreadgroup:MTLSizeMake(threadgroup_width,
                                                  threadgroup_height, 1)];
    }
    [encoder endEncoding];
    // Submit one command buffer containing all shadow generations.
    [command_buffer commit];
    pending_shadows.clear();
}

} // namespace

// =============================================================================
// Factory
// =============================================================================

namespace mosaic {

std::unique_ptr<ShadowManager>
create_metal_shadow_manager(id<MTLDevice> device,
                            id<MTLCommandQueue> command_queue) {
    if (!device || !command_queue) return nullptr;
    auto manager = std::make_unique<MetalShadowManager>(device, command_queue);
    if (!manager->valid()) return nullptr;
    return manager;
}

} // namespace mosaic
