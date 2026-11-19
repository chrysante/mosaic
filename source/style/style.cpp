#include "style/style.h"

#include <csp.hpp>
#include <imgui.h>
#include <vml/vml.hpp>

#include "gui/ui_context.h"
#include "service/style_system.h"
#include "style/style_class.h"
#include "style/style_desc.h"
#include "style/style_internal.h"

using namespace mosaic;

Style::Style() {
    auto& style_class = StyleClass::get();
    m_colors.resize(style_class.num_style_vars(StyleVarType::color));
    m_control_colors.resize(
        style_class.num_style_vars(StyleVarType::control_color));
}

StyleClass& StyleClass::get() {
    static StyleClass g_style_class;
    return g_style_class;
}

StyleColorId mosaic::register_style_color(StyleVarMetadata metadata) {
    return StyleClass::get().register_style_var<StyleVarType::color>(
        std::move(metadata));
}

StyleControlColorId
mosaic::register_style_control_color(StyleVarMetadata metadata) {
    return StyleClass::get().register_style_var<StyleVarType::control_color>(
        std::move(metadata));
}

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

Style StyleContext::make_style(StyleDesc const& desc, StyleType style_type) {
    unsigned const type_index = (unsigned)style_type;
    Style result;
    for (std::size_t i = 0; i < desc.colors.size(); ++i)
        result.m_colors[i] = convert(desc.colors[i][type_index]);
    for (std::size_t i = 0; i < desc.control_colors.size(); ++i)
        result.m_control_colors[i] =
            convert(desc.control_colors[i][type_index]);
    return result;
}

static ImVec4 to_v4(Color color) {
    return ImGui::ColorConvertU32ToFloat4(color.value);
}

void mosaic::apply_to_imgui(Style const& style) {
    auto& imgui_style = ImGui::GetStyle();
    auto& colors = imgui_style.Colors;
    auto clear = [&](ImGuiCol_ dest) { colors[dest] = {}; };
    /* base colors */ {
        auto map = csp::overload{
            [&](ImGuiCol_ dest, StyleColorId source) {
                colors[dest] = to_v4(style.get_color(source));
            },
            [&](ImGuiCol_ dest, StyleControlColorId source,
                Color ControlColor::* member) {
                colors[dest] = to_v4(style.get_color(source).*member);
            },
        };
        map(ImGuiCol_Text, style_color::text);
        map(ImGuiCol_TextDisabled, style_color::text_disabled);
        map(ImGuiCol_InputTextCursor, style_color::text);
        map(ImGuiCol_TextLink, style_color::text_link);

        map(ImGuiCol_WindowBg, style_color::window_background);
        map(ImGuiCol_ChildBg, style_color::window_background);
        map(ImGuiCol_PopupBg, style_color::window_background);
        map(ImGuiCol_Border, style_color::border);
        clear(ImGuiCol_BorderShadow);
        map(ImGuiCol_TitleBg, style_color::window_background);
        map(ImGuiCol_TitleBgActive, style_color::window_background);
        map(ImGuiCol_TitleBgCollapsed, style_color::window_background);
        map(ImGuiCol_MenuBarBg, style_color::window_background);
        clear(ImGuiCol_ScrollbarBg);
        map(ImGuiCol_ScrollbarGrab, style_color::scrollbar);
        map(ImGuiCol_ScrollbarGrabHovered, style_color::scrollbar);
        map(ImGuiCol_ScrollbarGrabActive, style_color::scrollbar);
        map(ImGuiCol_CheckMark, style_color::control_accent,
            &ControlColor::background);
        map(ImGuiCol_CheckboxSelectedBg, style_color::control,
            &ControlColor::background);
        map(ImGuiCol_SliderGrab, style_color::control_accent,
            &ControlColor::background);
        map(ImGuiCol_SliderGrabActive, style_color::control_accent,
            &ControlColor::background_active);
        map(ImGuiCol_Separator, style_color::separator);
        map(ImGuiCol_SeparatorHovered, style_color::separator);
        map(ImGuiCol_SeparatorActive, style_color::separator);
        map(ImGuiCol_ResizeGrip, style_color::separator);
        map(ImGuiCol_ResizeGripHovered, style_color::separator);
        map(ImGuiCol_ResizeGripActive, style_color::separator);
        map(ImGuiCol_DockingPreview, style_color::docking_preview);
        map(ImGuiCol_DockingEmptyBg, style_color::window_background);
        map(ImGuiCol_PlotLines, style_color::text);
        map(ImGuiCol_PlotLinesHovered, style_color::text);
        map(ImGuiCol_PlotHistogram, style_color::control_accent,
            &ControlColor::background);
        map(ImGuiCol_PlotHistogramHovered, style_color::control_accent,
            &ControlColor::background);
        // map(ImGuiCol_TableHeaderBg, );
        // map(ImGuiCol_TableBorderStrong, );
        // map(ImGuiCol_TableBorderLight, );
        // map(ImGuiCol_TableRowBg, );
        // map(ImGuiCol_TableRowBgAlt, );
        // map(ImGuiCol_TextSelectedBg, );
        map(ImGuiCol_TreeLines, style_color::text_disabled);
        map(ImGuiCol_NavCursor, style_color::control_accent,
            &ControlColor::background);
        map(ImGuiCol_DragDropTarget, style_color::control_accent,
            &ControlColor::background);
        clear(ImGuiCol_DragDropTargetBg);
        map(ImGuiCol_UnsavedMarker, style_color::text);
        // map(ImGuiCol_NavWindowingHighlight, );
        // map(ImGuiCol_NavWindowingDimBg, );
        // map(ImGuiCol_ModalWindowDimBg, );
    }
    /* control colors */ {
        auto map = [&](ImGuiCol_ dest_base, ImGuiCol_ dest_hovered,
                       ImGuiCol_ dest_active, StyleControlColorId source) {
            auto source_col = style.get_color(source);
            colors[dest_base] = to_v4(source_col.background);
            colors[dest_hovered] = to_v4(source_col.background_hovered);
            colors[dest_active] = to_v4(source_col.background_active);
        };
        map(ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive,
            style_color::control);
        map(ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
            style_color::control);
        map(ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive,
            style_color::control_accent);
        map(ImGuiCol_TabDimmed, ImGuiCol_TabHovered, ImGuiCol_TabDimmedSelected,
            style_color::control_tab_inactive);
        map(ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabSelected,
            style_color::control_tab);
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

Style const& mosaic::get_global_style() {
    return get_global_style_context().style();
}

StyleContext& mosaic::get_global_style_context() {
    return get_current_ui_context().style_system.get_context();
}

// clang-format off
StyleColorId const style_color::text              = register_style_color({.group = "Core", .name = "Text"});
StyleColorId const style_color::text_disabled     = register_style_color({.group = "Core", .name = "TextDisabled"});
StyleColorId const style_color::text_link         = register_style_color({.group = "Core", .name = "TextLink"});
StyleColorId const style_color::border            = register_style_color({.group = "Core", .name = "Border"});
StyleColorId const style_color::separator         = register_style_color({.group = "Core", .name = "Separator"});
StyleColorId const style_color::scrollbar         = register_style_color({.group = "Core", .name = "Scrollbar"});
StyleColorId const style_color::docking_preview   = register_style_color({.group = "Core", .name = "DockingPreview"});
StyleColorId const style_color::window_background = register_style_color({.group = "Core", .name = "WindowBackground"});

StyleControlColorId const style_color::control              = register_style_control_color({.group = "Core", .name = "Control"});
StyleControlColorId const style_color::control_accent       = register_style_control_color({.group = "Core", .name = "ControlAccent"});
StyleControlColorId const style_color::control_toolbar      = register_style_control_color({.group = "Core", .name = "ControlToolbar"});
StyleControlColorId const style_color::control_tab          = register_style_control_color({.group = "Core", .name = "ControlTab"});
StyleControlColorId const style_color::control_tab_inactive = register_style_control_color({.group = "Core", .name = "ControlTabInactive"});
// clang-format on

void mosaic::push_style_color(StyleColorId id, Color value) {
    get_global_style_context().push_color(id, value);
}

void mosaic::pop_style_color() { get_global_style_context().pop_color(); }

void mosaic::push_style_control_color(StyleControlColorId id,
                                      ControlColor value) {
    get_global_style_context().push_control_color(id, value);
}

void mosaic::pop_style_control_color() {
    get_global_style_context().pop_control_color();
}
