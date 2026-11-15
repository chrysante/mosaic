#pragma once

#include <span>
#include <stdint.h>

#include <utl/hash.hpp>
#include <utl/vector.hpp>

#include "ext/stb_rect_pack.h"
#include "imgui.h"
#include "support/draw_utils.h"

namespace mosaic {

// =============================================================================
// Descriptor cache
// =============================================================================

struct ShadowDescHash {
    size_t operator()(ShadowDesc const& desc) const noexcept {
        return utl::hash_combine(desc.corner_radius, desc.distance,
                                 desc.spread);
    }
};

struct ShadowDescEqual {
    bool operator()(ShadowDesc const& a, ShadowDesc const& b) const noexcept {
        return a.corner_radius == b.corner_radius && a.distance == b.distance &&
               a.spread == b.spread;
    }
};

// Compute shader parameters
// This layout must be kept in sync with `ShadowParams` in the shader.
struct ShadowParams {
    float corner_radius;
    float distance;
    float spread;

    uint32_t texture_width;
    uint32_t texture_height;

    uint32_t allocation_width;
    uint32_t allocation_height;

    uint32_t position_in_atlas_x;
    uint32_t position_in_atlas_y;

    uint32_t gutter;
    uint32_t falloff_distance;
};

struct AtlasPage;

struct ShadowBakeJob {
    AtlasPage* page = nullptr;
    ShadowParams params{};
};

class ShadowManager {
public:
    static constexpr int atlas_width = 1024;
    static constexpr int atlas_height = 1024;
    static constexpr int atlas_gutter = 1;
    static constexpr int patch_center_size = 2;

    virtual ~ShadowManager() = default;

    ShadowTextureHandle get_shadow_texture(ShadowDesc const& desc);

    void end_frame();

#ifndef NDEBUG
    void draw_debugger();
#endif

protected:
    struct RectAllocationResult {
        AtlasPage* page;
        stbrp_rect rect;
    };

private:
    virtual bool valid() const = 0;
    virtual bool ensure_pipeline() = 0;
    // result.page will be NULL ob failure
    virtual RectAllocationResult allocate_rect(int width, int height) = 0;
    virtual ImTextureID get_atlas_texture_id(AtlasPage* page) = 0;
    virtual utl::small_vector<AtlasPage*> get_atlas_pages() = 0;
    virtual void submit_jobs(std::span<ShadowBakeJob const> jobs) = 0;

    ShadowTextureHandle create_shadow(ShadowDesc const& desc);

    std::vector<ShadowBakeJob> pending_jobs;
    std::unordered_map<ShadowDesc, ShadowTextureHandle, ShadowDescHash,
                       ShadowDescEqual>
        cache;
};

} // namespace mosaic