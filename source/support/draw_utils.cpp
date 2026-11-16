#include "support/draw_utils.h"

#include <imgui.h>

#include "ext/stb_rect_pack.h"
#include "gui/ui_context.h"
#include "support/draw_utils_internal.h"

using namespace mosaic;

ShadowTextureHandle ShadowManager::get_shadow_texture(ShadowDesc const& desc) {
    if (!valid()) return {};
    if (desc.corner_radius < 0 || desc.distance <= 0) return {};
    if (auto itr = cache.find(desc); itr != cache.end()) return itr->second;
    return create_shadow(desc);
}

void ShadowManager::end_frame() {
    if (pending_jobs.empty()) return;
    submit_jobs(pending_jobs);
    pending_jobs.clear();
}

ShadowTextureHandle ShadowManager::create_shadow(ShadowDesc const& desc) {
    if (!ensure_pipeline()) return {};
    int const corner_radius = desc.corner_radius;
    int const falloff_distance = std::max(0, desc.distance) + 1;
    int const shape_width = corner_radius * 2 + patch_center_size;
    int const shape_height = corner_radius * 2 + patch_center_size;
    int const texture_width = shape_width + falloff_distance * 2;
    int const texture_height = shape_height + falloff_distance * 2;
    int const allocation_width = texture_width + atlas_gutter * 2;
    int const allocation_height = texture_height + atlas_gutter * 2;
    if (allocation_width > atlas_width || allocation_height > atlas_height)
        return {};
    auto [page, rect] = allocate_rect(allocation_width, allocation_height);
    if (!page) return {};
    // The actual shadow texture starts after the gutter.
    int const texture_x = rect.x + atlas_gutter;
    int const texture_y = rect.y + atlas_gutter;
    // clang-format off
    pending_jobs.push_back({
        .page = page,
        .params = {
            .corner_radius = (float)desc.corner_radius,
            .distance = (float)desc.distance,
            .texture_width = (uint32_t)texture_width,
            .texture_height = (uint32_t)texture_height,
            .allocation_width = (uint32_t)allocation_width,
            .allocation_height = (uint32_t)allocation_height,
            .position_in_atlas_x = (uint32_t)rect.x,
            .position_in_atlas_y = (uint32_t)rect.y,
            .gutter = (uint32_t)atlas_gutter,
            .falloff_distance = (uint32_t)falloff_distance
        }
    });
    ShadowTextureHandle handle = {
        .texture_id = get_atlas_texture_id(page),
        .uv_min = {(float)texture_x / (float)atlas_width,
                   (float)texture_y / (float)atlas_height},
        .uv_max = {(float)(texture_x + texture_width) / (float)atlas_width,
                   (float)(texture_y + texture_height) / (float)atlas_height},
        .falloff_distance = falloff_distance,
        .corner_radius = corner_radius,
        .atlas_size = {atlas_width, atlas_height}
    };
    // clang-format on
    cache.emplace(desc, handle);
    return handle;
}

void ShadowManager::draw_debugger() {
    ImGui::PushID(this);
    auto atlas_pages = get_atlas_pages();
    static int page_index = 0;
    ImGui::SliderInt(
        "Page", &page_index, 0, (int)atlas_pages.size() - 1, nullptr,
        ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange);
    if (page_index >= 0 && page_index < (int)atlas_pages.size()) {
        AtlasPage* page = atlas_pages[(std::size_t)page_index];
        float size_avail = ImGui::GetContentRegionAvail().x;
        ImTextureID texture_id = get_atlas_texture_id(page);
        vml::float2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, pos + vml::float2(size_avail), IM_COL32_BLACK);
        ImGui::Image(texture_id, {size_avail, size_avail});
        for (auto& [desc, handle] : cache) {
            if (handle.texture_id != texture_id) continue;
        }
    }
    ImGui::PopID();
}

using namespace vml::short_types;

void draw_shadow_impl(ImDrawList* draw_list, ShadowTextureHandle const& handle,
                      float2 position, float2 size,
                      ShadowDrawOptions const& options) {
    position += options.scale * vml::float2(-options.spread) + options.offset;
    size += vml::float2(2 * options.scale * options.spread);

    float2 uv0 = handle.uv_min;
    float2 uv3 = handle.uv_max;

    // Reconstruct the nine-patch boundaries.
    float const border_size = handle.falloff_distance + handle.corner_radius;
    float2 uv1 = uv0 + float2(border_size) / handle.atlas_size;
    float2 uv2 = uv3 - float2(border_size) / handle.atlas_size;

    // Shadow bounds in screen space.
    float scaled_falloff = options.scale * handle.falloff_distance;
    float2 min = position - float2(scaled_falloff);
    float2 max = position + size + float2(scaled_falloff);

    // Original corner region dimensions.
    float const left = options.scale * border_size;
    float const right = options.scale * border_size;
    float const top = options.scale * border_size;
    float const bottom = options.scale * border_size;

    // Available destination dimensions.
    float const width = max.x - min.x;
    float const height = max.y - min.y;

    // Prevent opposing corner regions from overlapping.
    float const dst_left = std::min(left, width * 0.5f);
    float const dst_right = std::min(right, width * 0.5f);
    float const dst_top = std::min(top, height * 0.5f);
    float const dst_bottom = std::min(bottom, height * 0.5f);

    float2 x[] = {min, float2(min.x + dst_left, min.y),
                  float2(max.x - dst_right, min.y), max};
    float2 y[] = {min, float2(min.x, min.y + dst_top),
                  float2(min.x, max.y - dst_bottom), max};
    float2 u[] = {uv0, uv1, uv2, uv3};
    float2 v[] = {uv0, uv1, uv2, uv3};

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float2 p_min(x[col].x, y[row].y);
            float2 p_max(x[col + 1].x, y[row + 1].y);
            if (p_max.x <= p_min.x || p_max.y <= p_min.y) continue;
            draw_list->AddImage(
                handle.texture_id, p_min, p_max, float2(u[col].x, v[row].y),
                float2(u[col + 1].x, v[row + 1].y), options.tint);
        }
    }
}

void mosaic::draw_shadow(ImDrawList* draw_list, vml::float2 position,
                         vml::float2 size, ShadowDrawOptions const& options) {
    auto* shadow_manager = get_current_ui_context().shadow_manager;
    if (!shadow_manager) return;
    auto handle = shadow_manager->get_shadow_texture(
        {.corner_radius = (int)std::ceil(options.corner_radius),
         .distance = (int)std::ceil(options.distance)});
    if (!handle.texture_id) return;
    draw_shadow_impl(draw_list, handle, position, size, options);
}