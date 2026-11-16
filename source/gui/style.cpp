#include "gui/style.h"

#include <csp.hpp>
#include <imgui.h>
#include <vml/vml.hpp>

#include "gui/style_desc.h"
#include "gui/ui_context.h"
#include "service/style_system.h"

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
    return {.background = convert(desc.background),
            .background_hovered = convert(desc.background_hovered),
            .background_active = convert(desc.background_active),
            .border = convert(desc.border),
            .text = convert(desc.text)};
}

Style_ mosaic::compute_from_desc(StyleDesc const& desc, StyleType style_type) {
    unsigned i = (unsigned)style_type;
    Style_ result;
    result.style_type = style_type;
    for (std::size_t j = 0; j < enum_count<StyleColor>; ++j) {
        StyleColor index = (StyleColor)j;
        result.color[index] = convert(desc.color[i][index]);
    }
    for (std::size_t j = 0; j < enum_count<StyleColorControl>; ++j) {
        StyleColorControl index = (StyleColorControl)j;
        result.control_color[index] = convert(desc.control_color[i][index]);
    }
    return result;
}

static ImVec4 to_v4(Color color) {
    return ImGui::ColorConvertU32ToFloat4(color.value);
}

void mosaic::apply_to_imgui(Style_ const& style) {
    auto& imgui_style = ImGui::GetStyle();
    auto& colors = imgui_style.Colors;
    auto clear = [&](ImGuiCol_ dest) { colors[dest] = {}; };
    /* base colors */ {
        auto map = csp::overload{
            [&](ImGuiCol_ dest, StyleColor source) {
                colors[dest] = to_v4(style.color[source]);
            },
            [&](ImGuiCol_ dest, StyleColorControl source,
                Color ControlColor::* member) {
                colors[dest] = to_v4(style.control_color[source].*member);
            },
        };
        map(ImGuiCol_Text, StyleColor::text);
        map(ImGuiCol_TextDisabled, StyleColor::text_disabled);
        map(ImGuiCol_InputTextCursor, StyleColor::text);
        map(ImGuiCol_TextLink, StyleColor::text_link);

        map(ImGuiCol_WindowBg, StyleColor::window_background);
        map(ImGuiCol_ChildBg, StyleColor::window_background);
        map(ImGuiCol_PopupBg, StyleColor::window_background);
        map(ImGuiCol_Border, StyleColor::border);
        clear(ImGuiCol_BorderShadow);
        map(ImGuiCol_TitleBg, StyleColor::window_background);
        map(ImGuiCol_TitleBgActive, StyleColor::window_background);
        map(ImGuiCol_TitleBgCollapsed, StyleColor::window_background);
        map(ImGuiCol_MenuBarBg, StyleColor::window_background);
        clear(ImGuiCol_ScrollbarBg);
        map(ImGuiCol_ScrollbarGrab, StyleColor::scrollbar);
        map(ImGuiCol_ScrollbarGrabHovered, StyleColor::scrollbar);
        map(ImGuiCol_ScrollbarGrabActive, StyleColor::scrollbar);
        map(ImGuiCol_CheckMark, StyleColorControl::accent,
            &ControlColor::background);
        map(ImGuiCol_CheckboxSelectedBg, StyleColorControl::Default,
            &ControlColor::background);
        map(ImGuiCol_SliderGrab, StyleColorControl::accent,
            &ControlColor::background);
        map(ImGuiCol_SliderGrabActive, StyleColorControl::accent,
            &ControlColor::background_active);
        map(ImGuiCol_Separator, StyleColor::separator);
        map(ImGuiCol_SeparatorHovered, StyleColor::separator);
        map(ImGuiCol_SeparatorActive, StyleColor::separator);
        map(ImGuiCol_ResizeGrip, StyleColor::separator);
        map(ImGuiCol_ResizeGripHovered, StyleColor::separator);
        map(ImGuiCol_ResizeGripActive, StyleColor::separator);
        map(ImGuiCol_DockingPreview, StyleColor::docking_preview);
        map(ImGuiCol_DockingEmptyBg, StyleColor::window_background);
        map(ImGuiCol_PlotLines, StyleColor::text);
        map(ImGuiCol_PlotLinesHovered, StyleColor::text);
        map(ImGuiCol_PlotHistogram, StyleColorControl::accent,
            &ControlColor::background);
        map(ImGuiCol_PlotHistogramHovered, StyleColorControl::accent,
            &ControlColor::background);
        // map(ImGuiCol_TableHeaderBg, );
        // map(ImGuiCol_TableBorderStrong, );
        // map(ImGuiCol_TableBorderLight, );
        // map(ImGuiCol_TableRowBg, );
        // map(ImGuiCol_TableRowBgAlt, );
        // map(ImGuiCol_TextSelectedBg, );
        map(ImGuiCol_TreeLines, StyleColor::text_disabled);
        map(ImGuiCol_NavCursor, StyleColorControl::accent,
            &ControlColor::background);
        map(ImGuiCol_DragDropTarget, StyleColorControl::accent,
            &ControlColor::background);
        clear(ImGuiCol_DragDropTargetBg);
        map(ImGuiCol_UnsavedMarker, StyleColor::text);
        // map(ImGuiCol_NavWindowingHighlight, );
        // map(ImGuiCol_NavWindowingDimBg, );
        // map(ImGuiCol_ModalWindowDimBg, );
    }
    /* control colors */ {
        auto map = [&](ImGuiCol_ dest_base, ImGuiCol_ dest_hovered,
                       ImGuiCol_ dest_active, StyleColorControl source) {
            auto source_col = style.control_color[source];
            colors[dest_base] = to_v4(source_col.background);
            colors[dest_hovered] = to_v4(source_col.background_hovered);
            colors[dest_active] = to_v4(source_col.background_active);
        };
        map(ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive,
            StyleColorControl::Default);
        map(ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
            StyleColorControl::Default);
        map(ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive,
            StyleColorControl::accent);
        map(ImGuiCol_TabDimmed, ImGuiCol_TabHovered, ImGuiCol_TabDimmedSelected,
            StyleColorControl::tab_inactive);
        map(ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabSelected,
            StyleColorControl::tab);
        clear(ImGuiCol_TabSelectedOverline);
        clear(ImGuiCol_TabDimmedSelectedOverline);
    }
    /* style vars */ {
        imgui_style.WindowBorderSize = 0.0f;
        imgui_style.ChildBorderSize = 0.0f;
        imgui_style.PopupBorderSize = 0.0f;
        imgui_style.FrameBorderSize = 0.0f;
        imgui_style.TabBorderSize = 0.0f;
        imgui_style.TabBarBorderSize = 0.0f;
        imgui_style.WindowRounding = 0.0f;
        imgui_style.ChildRounding = 0.0f;
        imgui_style.PopupRounding = 0.0f;
        imgui_style.TabRounding = 0.0f;

        imgui_style.FrameRounding = 6.0f;
        imgui_style.SelectableRounding = 5.0f;
        imgui_style.GrabMinSize = 12.0f;
        imgui_style.GrabRounding = 6.0f;
        imgui_style.ScrollbarSize = 10.0f;
        imgui_style.ScrollbarRounding = 10.0f;
        imgui_style.WindowTitleAlign = {0.5f, 0.5f};
        imgui_style.CircleTessellationMaxError = 0.1;
    }
}

static StyleContext& get_style_context() {
    return get_current_ui_context().style_system.active_style_contex();
}

Style_ const& mosaic::get_global_style2() {
    return get_style_context().style();
}

void mosaic::push_style_color(StyleColor index, Color value) {
    get_style_context().push_color(index, value);
}

void mosaic::pop_style_color() { get_style_context().pop_color(); }

void mosaic::push_style_control_color(StyleColorControl index,
                                      ControlColor value) {
    get_style_context().push_control_color(index, value);
}

void mosaic::pop_style_control_color() {
    get_style_context().pop_control_color();
}
