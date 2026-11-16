#pragma once

#include <imgui.h>

#include <vml/vml.hpp>

namespace mosaic {

/// Parameter structure for function `draw_shadow()`
struct ShadowDrawOptions {
    /// Corner radius of the rectangle casting the shadow.
    float corner_radius = 0.0f;

    /// Number of pixels the shadow extends outwards from the casting rectangle.
    float distance = 20.0f;

    /// Expands or contracts the shadow-casting shape before generating the
    /// shadow.
    float spread = 0.0f;

    /// Offset applied to the shadow texture
    vml::float2 offset = {0.0f, 0.0f};

    /// Scale applied to the shadow geometry, including its corners, distance,
    /// spread and offset. Does not affect the position or size of the casting
    /// rectangle.
    float scale = 1.0f;

    /// Shadow color
    ImU32 tint = IM_COL32(0, 0, 0, 180);
};

/// Draws a shadow around a rectangle.
///
/// \p draw_list The ImGui draw list that the shadow texture is appended to
/// \p position The top-left corner of the rectangle in screen space.
/// \p size The size of the rectangle casting the shadow.
/// \p options Rendering options
void draw_shadow(ImDrawList* draw_list, vml::float2 position, vml::float2 size,
                 ShadowDrawOptions const& options);

} // namespace mosaic