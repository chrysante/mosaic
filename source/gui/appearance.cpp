#include "gui/appearance.h"

#include <imgui.h>

#include "gui/appearance_internal.h"

using namespace mosaic;
using namespace vml::short_types;

static Style& g_style() {
    static Style style = make_style(get_system_appearance());
    return style;
}

Style mosaic::get_global_style() { return g_style(); }

static void apply_style_to_imgui(Style const& style) {
    auto& imgui_style = ImGui::GetStyle();
    auto* colors = imgui_style.Colors;
    colors[ImGuiCol_Text] = style.colors.text;
    colors[ImGuiCol_TextDisabled] = style.colors.text_disabled;
    colors[ImGuiCol_WindowBg] = style.colors.window_bg;
    colors[ImGuiCol_ChildBg] = style.colors.child_window_bg;
    colors[ImGuiCol_PopupBg] = style.colors.popup_bg;
    colors[ImGuiCol_Border] = style.colors.border;
    colors[ImGuiCol_BorderShadow] = {};
    colors[ImGuiCol_FrameBg] = style.colors.control_frame;
    colors[ImGuiCol_FrameBgHovered] = style.colors.control_frame_hovered;
    colors[ImGuiCol_FrameBgActive] = style.colors.control_frame_down;
    colors[ImGuiCol_TitleBg] = style.colors.window_title_bar;
    colors[ImGuiCol_TitleBgActive] = style.colors.active_window_title_bar;
    colors[ImGuiCol_TitleBgCollapsed] = style.colors.window_title_bar;
    colors[ImGuiCol_MenuBarBg] = style.colors.menubar_bg;
    colors[ImGuiCol_ScrollbarBg] = {};
    colors[ImGuiCol_ScrollbarGrab] = style.colors.scrollbar;
    colors[ImGuiCol_ScrollbarGrabHovered] = style.colors.scrollbar;
    colors[ImGuiCol_ScrollbarGrabActive] = style.colors.scrollbar;
    colors[ImGuiCol_CheckMark] = style.colors.check_mark;
    colors[ImGuiCol_SliderGrab] = style.colors.highlight_control_frame;
    colors[ImGuiCol_SliderGrabActive] =
        style.colors.highlight_control_frame_down;
    colors[ImGuiCol_Button] = style.colors.control_frame;
    colors[ImGuiCol_ButtonHovered] = style.colors.control_frame_hovered;
    colors[ImGuiCol_ButtonActive] = style.colors.control_frame_down;
    colors[ImGuiCol_Header] = style.colors.highlight_control_frame;
    colors[ImGuiCol_HeaderHovered] =
        style.colors.highlight_control_frame_hovered;
    colors[ImGuiCol_HeaderActive] = style.colors.highlight_control_frame_down;
    colors[ImGuiCol_Separator] = style.colors.separator;
    colors[ImGuiCol_SeparatorHovered] = style.colors.separator;
    colors[ImGuiCol_SeparatorActive] = style.colors.separator;
    colors[ImGuiCol_ResizeGrip] = style.colors.separator;
    colors[ImGuiCol_ResizeGripHovered] = style.colors.separator;
    colors[ImGuiCol_ResizeGripActive] = style.colors.separator;
    colors[ImGuiCol_TableBorderLight] = style.colors.separator;
    colors[ImGuiCol_TableBorderStrong] = style.colors.separator;
    colors[ImGuiCol_InputTextCursor] = style.colors.text;
    colors[ImGuiCol_Tab] = style.colors.tab;
    colors[ImGuiCol_TabHovered] = style.colors.tab_hovered;
    colors[ImGuiCol_TabActive] = style.colors.tab_active;
    colors[ImGuiCol_TabUnfocused] = style.colors.tab_unfocused;
    colors[ImGuiCol_TabUnfocusedActive] = style.colors.tab_unfocused_active;
    colors[ImGuiCol_DockingPreview] = style.colors.docking_preview;
    colors[ImGuiCol_DockingEmptyBg] = style.colors.window_bg;
    colors[ImGuiCol_DragDropTarget] = style.colors.drag_drop_target;
    colors[ImGuiCol_UnsavedMarker] = style.colors.text;
    imgui_style.WindowBorderSize = style.vars.window_border ? 1.0 : 0.0;
    imgui_style.ChildBorderSize = style.vars.child_window_border ? 1.0 : 0.0;
    imgui_style.PopupBorderSize = style.vars.popup_border ? 1.0 : 0.0;
    imgui_style.FrameBorderSize = style.vars.frame_border ? 1.0 : 0.0;
    imgui_style.TabBorderSize = style.vars.tab_border ? 1.0 : 0.0;
    imgui_style.TabBarBorderSize = style.vars.tab_bar_border ? 1.0 : 0.0;
    imgui_style.WindowRounding = style.vars.window_rounding;
    imgui_style.ChildRounding = style.vars.child_window_rounding;
    imgui_style.PopupRounding = style.vars.popup_rounding;
    imgui_style.FrameRounding = style.vars.frame_rounding;
    imgui_style.SelectableRounding = style.vars.selectable_rounding;
    imgui_style.TabRounding = style.vars.tab_rounding;
    imgui_style.GrabMinSize = style.vars.grab_min_size;
    imgui_style.GrabRounding = style.vars.grab_rounding;
    imgui_style.ScrollbarSize = style.vars.scrollbar_size;
    imgui_style.ScrollbarRounding = style.vars.scrollbar_rounding;
    imgui_style.WindowTitleAlign = style.vars.window_title_align;
    imgui_style.CircleTessellationMaxError = 0.1;
}

void mosaic::update_style(bool force) {
    auto& style = g_style();
    auto type = get_system_appearance();
    if (!force && style.type == type) return;
    style = make_style(type);
    apply_style_to_imgui(style);
}
