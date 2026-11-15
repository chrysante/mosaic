#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#import <Metal/Metal.h>
#include <imgui.h>

#include "ext/stb_rect_pack.h"
#include "support/draw_utils.h"
#include "support/draw_utils_internal.h"

using namespace mosaic;

struct mosaic::AtlasPage {
    id<MTLTexture> texture = nil;

    std::vector<stbrp_node> nodes;
    stbrp_context pack_context{};

    explicit AtlasPage(id<MTLDevice> device) {
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:ShadowManager::atlas_width
                                        height:ShadowManager::atlas_height
                                     mipmapped:NO];
        descriptor.usage =
            MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        descriptor.storageMode = MTLStorageModePrivate;
        texture = [device newTextureWithDescriptor:descriptor];
        if (!texture) return;
        texture.label = @"ImGui Shadow Atlas";
        nodes.resize(ShadowManager::atlas_width);
        stbrp_init_target(&pack_context, ShadowManager::atlas_width,
                          ShadowManager::atlas_height, nodes.data(),
                          (int)nodes.size());
    }

    bool valid() const { return texture != nil; }

    std::optional<stbrp_rect> allocate_rect(int width, int height) {
        stbrp_rect rect = {.w = width, .h = height};
        if (!stbrp_pack_rects(&pack_context, &rect, 1) || rect.was_packed == 0)
            return std::nullopt;
        return rect;
    }
};

namespace {

struct MetalShadowManager final : ShadowManager {
    MetalShadowManager(id<MTLDevice> device, id<MTLCommandQueue> command_queue)
        : device(device)
        , command_queue(command_queue) {}

    bool valid() const override {
        return device != nil && command_queue != nil;
    }

    bool ensure_pipeline() override;

    AtlasPage* create_atlas_page();

    RectAllocationResult allocate_rect(int width, int height) override;

    ImTextureID get_atlas_texture_id(AtlasPage* page) override {
        assert(page);
        return (ImTextureID)(intptr_t)(__bridge void*)page->texture;
    }

    void submit_jobs(std::span<ShadowBakeJob const> jobs) override;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
    id<MTLComputePipelineState> pipeline = nil;
    std::vector<std::unique_ptr<AtlasPage>> atlas_pages;
};

} // namespace

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

AtlasPage* MetalShadowManager::create_atlas_page() {
    auto page = std::make_unique<AtlasPage>(device);
    if (!page->valid()) return nullptr;
    AtlasPage* result = page.get();
    atlas_pages.emplace_back(std::move(page));
    return result;
}

ShadowManager::RectAllocationResult
MetalShadowManager::allocate_rect(int width, int height) {
    for (auto& page : atlas_pages)
        if (auto rect = page->allocate_rect(width, height))
            return {page.get(), rect.value()};
    AtlasPage* page = create_atlas_page();
    if (!page) return {};
    auto rect = page->allocate_rect(width, height);
    if (!rect) return {};
    return {page, rect.value()};
}

static constexpr uint32_t threadgroup_width = 8;
static constexpr uint32_t threadgroup_height = 8;

void MetalShadowManager::submit_jobs(std::span<ShadowBakeJob const> jobs) {
    assert(!jobs.empty());
    if (!pipeline) return;
    id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
    if (!command_buffer) return;
    command_buffer.label = @"ImGui Shadow Generation";
    id<MTLComputeCommandEncoder> encoder =
        [command_buffer computeCommandEncoder];
    if (!encoder) return;
    encoder.label = @"Shadow SDF Generation";
    [encoder setComputePipelineState:pipeline];
    for (ShadowBakeJob const& job : jobs) {
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
    [command_buffer commit];
}

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
