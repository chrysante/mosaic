#pragma once

#include <imgui.h>

#include <vml/vml.hpp>

namespace mosaic {

struct ShadowDesc {
    /// Corner radius of the rectangle casting the shadow.
    int corner_radius = 0;

    /// Number of pixels the shadow extends outwards from the casting rectangle.
    int distance = 20;

    /// Expands or contracts the shadow-casting shape before generating the
    /// shadow.
    int spread = 0;
};

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

/// Queries the applications shadow manager for a shadow texture. Shadow
/// textures are cached and baked lazily at the end of each frame.
ShadowTextureHandle get_shadow_texture(ShadowDesc const& desc);

/// Draws a shadow around a rectangle.
///
/// \p position is the top-left corner of the rectangle in screen space.
/// \p size is the size of the rectangle casting the shadow.
/// \p scale Scale applied to the shadow geometry, including its falloff and
///          corners. Does not affect the position or size of the casting
///          rectangle.
void draw_shadow(ImDrawList* draw_list, ShadowTextureHandle const& shadow,
                 vml::float2 position, vml::float2 size, float scale = 1.0f,
                 ImU32 tint = IM_COL32(0, 0, 0, 180));

} // namespace mosaic