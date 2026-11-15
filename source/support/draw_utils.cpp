#include "support/draw_utils.h"

#include <imgui.h>

#include "ext/stb_rect_pack.h"
#include "gui/ui_context.h"
#include "support/draw_utils_internal.h"

using namespace mosaic;

ShadowTextureHandle mosaic::get_shadow_texture(ShadowDesc const& desc) {
    auto* shadow_manager = get_current_ui_context().shadow_manager;
    if (!shadow_manager) return {};
    return shadow_manager->get_shadow_texture(desc);
}

ShadowTextureHandle ShadowManager::get_shadow_texture(ShadowDesc const& desc) {
    if (!valid()) return {};
    if (!std::isfinite(desc.corner_radius) ||
        !std::isfinite(desc.shadow_spread) ||
        !std::isfinite(desc.shadow_softness) || desc.corner_radius < 0.0f ||
        desc.shadow_softness <= 0.0f)
        return {};
    // Return an existing allocation.
    if (auto it = cache.find(desc); it != cache.end()) return it->second;
    // Compile the shader lazily, on the first actual request.
    return create_shadow(desc);
}

void ShadowManager::end_frame() {
    if (pending_jobs.empty()) return;
    submit_jobs(pending_jobs);
    pending_jobs.clear();
}

ShadowTextureHandle ShadowManager::create_shadow(ShadowDesc const& desc) {
    if (!ensure_pipeline()) return {};
    int const corner = (int)std::ceil(desc.corner_radius);
    int const padding = (int)std::ceil(
        std::max(0.0f, desc.shadow_spread + desc.shadow_softness) + 1.0f);
    int const shape_width = corner * 2 + patch_center_size;
    int const shape_height = corner * 2 + patch_center_size;
    int const texture_width = shape_width + padding * 2;
    int const texture_height = shape_height + padding * 2;
    int const allocation_width = texture_width + atlas_gutter * 2;
    int const allocation_height = texture_height + atlas_gutter * 2;
    if (allocation_width > atlas_width || allocation_height > atlas_height)
        return {};
    auto [page, rect] = allocate_rect(allocation_width, allocation_height);
    if (!page) return {};
    // The actual shadow texture starts after the gutter.
    int const texture_x = rect.x + atlas_gutter;
    int const texture_y = rect.y + atlas_gutter;

    ShadowBakeJob job{};
    job.page = page;
    job.params = {desc.corner_radius,
                  desc.shadow_spread,
                  desc.shadow_softness,
                  (uint32_t)texture_width,
                  (uint32_t)texture_height,
                  (uint32_t)allocation_width,
                  (uint32_t)allocation_height,
                  (uint32_t)rect.x,
                  (uint32_t)rect.y,
                  (uint32_t)atlas_gutter,
                  (uint32_t)padding};
    pending_jobs.emplace_back(job);

    ShadowTextureHandle handle{};
    handle.texture_id = get_atlas_texture_id(page);
    handle.uv_min = {(float)texture_x / (float)atlas_width,
                     (float)texture_y / (float)atlas_height};
    handle.uv_max = {(float)(texture_x + texture_width) / (float)atlas_width,
                     (float)(texture_y + texture_height) / (float)atlas_height};

    // Nine-patch boundaries, relative to the inner allocation.
    handle.patch_min = {(float)(padding + corner), (float)(padding + corner)};
    handle.patch_max = {(float)(padding + corner + patch_center_size),
                        (float)(padding + corner + patch_center_size)};
    handle.atlas_size = {(float)atlas_width, (float)atlas_height};
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
        ImGui::Image(texture_id, {size_avail, size_avail});
        for (auto& [desc, handle] : cache) {
            if (handle.texture_id != texture_id) continue;
        }
    }
    ImGui::PopID();
}

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
