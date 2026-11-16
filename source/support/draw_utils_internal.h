#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "ext/stb_rect_pack.h"
#include "imgui.h"
#include "support/draw_utils.h"

namespace mosaic {

// =============================================================================
// Shadow Rendering
// =============================================================================

/// Descriptor key for a shadow texture
struct ShadowDesc {
    int corner_radius;
    int distance;

    bool operator==(ShadowDesc const&) const = default;
};

} // namespace mosaic

template <> struct std::hash<mosaic::ShadowDesc> {
    std::size_t operator()(mosaic::ShadowDesc const& desc) const {
        return utl::hash_combine(desc.corner_radius, desc.distance);
    }
};

namespace mosaic {

struct ShadowTextureHandle {
    /// Atlas texture
    ImTextureID texture_id = {};

    /// Top-left UV coordinate of the allocation
    vml::float2 uv_min = {};

    /// Bottom-right UV coordinate of the allocation
    vml::float2 uv_max = {};

    /// Distance the shadow extends beyond the casting rectangle
    int falloff_distance = 0.0f;

    /// Corner radius of the shadow-casting rectangle
    int corner_radius = 0.0f;

    /// Size of the atlas containing the shadow texture
    vml::int2 atlas_size = {};
};

// Compute shader parameters
// This layout must be kept in sync with `ShadowParams` in the shader.
struct ShadowBakeParams {
    float corner_radius;
    float distance;

    std::uint32_t texture_width;
    std::uint32_t texture_height;

    std::uint32_t allocation_width;
    std::uint32_t allocation_height;

    std::uint32_t position_in_atlas_x;
    std::uint32_t position_in_atlas_y;

    std::uint32_t gutter;
    std::uint32_t falloff_distance;
};

struct AtlasPage;

struct ShadowBakeJob {
    AtlasPage* page = nullptr;
    ShadowBakeParams params{};
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
    utl::hashmap<ShadowDesc, ShadowTextureHandle> cache;
};

} // namespace mosaic