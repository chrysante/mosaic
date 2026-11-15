#include "support/draw_utils.h"

#include <imgui.h>

using namespace mosaic;

void mosaic::draw_shadow(ImDrawList* draw_list,
                         ShadowTextureHandle const& shadow,
                         vml::float2 position, vml::float2 size, ImU32 tint) {
    if (!shadow.texture_id) return;
    ImVec2 uv0 = shadow.uv_min;
    ImVec2 uv1 = shadow.uv_max;
    ImVec2 tex_size = ImVec2((uv1.x - uv0.x) * shadow.atlas_size.x,
                             (uv1.y - uv0.y) * shadow.atlas_size.y);
    ImVec2 u0 = uv0;
    ImVec2 u1 = ImVec2(uv0.x + shadow.patch_min.x / shadow.atlas_size.x,
                       uv0.y + shadow.patch_min.y / shadow.atlas_size.y);
    ImVec2 u2 = ImVec2(uv0.x + shadow.patch_max.x / shadow.atlas_size.x,
                       uv0.y + shadow.patch_max.y / shadow.atlas_size.y);
    ImVec2 u3 = uv1;
    // Keep the corner regions at their original pixel size.
    float left = shadow.patch_min.x;
    float top = shadow.patch_min.y;
    float right = tex_size.x - shadow.patch_max.x;
    float bottom = tex_size.y - shadow.patch_max.y;

    // FIXME: change hard-coded values
    auto min = position - vml::float2(12.0f, 12.0f);
    auto max = position + size + vml::float2(12.0f, 12.0f);

    ImVec2 x[] = {min, ImVec2(min.x + left, min.y),
                  ImVec2(max.x - right, min.y), max};
    ImVec2 y[] = {min, ImVec2(min.x, min.y + top),
                  ImVec2(min.x, max.y - bottom), max};
    ImVec2 u[] = {u0, u1, u2, u3};
    ImVec2 v[] = {u0, u1, u2, u3};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            draw_list->AddImage(shadow.texture_id, ImVec2(x[col].x, y[row].y),
                                ImVec2(x[col + 1].x, y[row + 1].y),
                                ImVec2(u[col].x, v[row].y),
                                ImVec2(u[col + 1].x, v[row + 1].y), tint);
}