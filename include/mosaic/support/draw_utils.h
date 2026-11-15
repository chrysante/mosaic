#pragma once

#include <imgui.h>

#include <vml/vml.hpp>

namespace mosaic {

struct ShadowDesc {
    /// Radius of the rounded rectangle being shadowed
    float corner_radius = 8.0f;

    /// Expands or contracts the underlying shape before evaluating the shadow
    float shadow_spread = 0.0f;

    /// Width of the shadow's falloff, in pixels
    float shadow_softness = 12.0f;
};

struct ShadowTextureHandle {
    /// Atlas texture
    ImTextureID texture_id = {};

    /// Top-left UV coordinate of the allocation
    vml::float2 uv_min = {};

    /// Bottom-right UV coordinate of the allocation
    vml::float2 uv_max = {};

    /// Top-left nine-patch boundary, in allocation-local pixels
    vml::float2 patch_min = {};

    /// Bottom-right nine-patch boundary, in allocation-local pixels
    vml::float2 patch_max = {};

    vml::float2 atlas_size = {};
};

class ShadowManager {
public:
    virtual ~ShadowManager() = default;

    virtual ShadowTextureHandle get_shadow_texture(ShadowDesc const& desc) = 0;

    virtual void end_frame() = 0;
};

/// Draws a shadow around a rectangle.
///
/// \p position is the top-left corner of the rectangle in screen space.
/// \p size is the size of the rectangle casting the shadow.
void draw_shadow(ImDrawList* draw_list, ShadowTextureHandle const& shadow,
                 vml::float2 position, vml::float2 size,
                 ImU32 tint = IM_COL32(0, 0, 0, 180));

} // namespace mosaic