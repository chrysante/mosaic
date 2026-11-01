#include "gui/style.h"

#include <imgui.h>
#include <vml/vml.hpp>

#include "gui/style_desc.h"
#include "vml/ext.hpp"

using namespace mosaic;

static Color convert(ColorDesc desc) { return {.value = desc.value}; }

static Color apply_mod(Color base, ColorModifier mod) {
    vml::float4 rgba = ImGui::ColorConvertU32ToFloat4(base.value);
    vml::float4 hsva = vml::rgba_to_hsva(rgba);
    hsva.saturation =
        std::clamp(hsva.saturation * mod.saturation_scale, 0.0f, 1.0f);
    hsva.value = std::clamp(hsva.value * mod.value_scale, 0.0f, 1.0f);
    hsva.alpha = std::clamp(hsva.alpha * mod.alpha_scale, 0.0f, 1.0f);
    rgba = vml::hsva_to_rgba(hsva);
    return {.value = ImGui::ColorConvertFloat4ToU32(rgba)};
}

static ControlColor convert(ControlColorDesc const& desc) {
    return {.base = convert(desc.base),
            .hovered = apply_mod(convert(desc.base), desc.hovered),
            .active = apply_mod(convert(desc.base), desc.active)};
}

Style_ mosaic::compute_from_desc(StyleDesc const& desc, StyleType style_type) {
    unsigned i = (unsigned)style_type;
    return {
        .color = {.control = convert(desc.color[i].control),
                  .control_accent = convert(desc.color[i].control_accent),
                  .window_background = convert(desc.color->window_background)}};
}

static ImVec4 to_v4(Color color) {
    return ImGui::ColorConvertU32ToFloat4(color.value);
}

void mosaic::apply_to_imgui(Style_ const& style) {
    auto& imgui_style = ImGui::GetStyle();
    auto& colors = imgui_style.Colors;
    colors[ImGuiCol_Button] = to_v4(style.color.control.base);
    colors[ImGuiCol_ButtonHovered] = to_v4(style.color.control.hovered);
    colors[ImGuiCol_ButtonActive] = to_v4(style.color.control.active);

    colors[ImGuiCol_FrameBg] = to_v4(style.color.control.base);
    colors[ImGuiCol_FrameBgHovered] = to_v4(style.color.control.hovered);
    colors[ImGuiCol_FrameBgActive] = to_v4(style.color.control.active);
}

static Style_ g_style;

Style_ const& mosaic::get_global_style2() { return g_style; }

Style_& mosaic::get_global_style_write() { return g_style; }
