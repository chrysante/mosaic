#include "gui/toolbar.h"

#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/uuid.hpp>
#include <utl/vector.hpp>

#include "gui/command_palette.h"
#include "gui/font.h"

using namespace mosaic;
using namespace vml::short_types;

static bool toolbar_button_impl(ToolbarButtonStyle const& style,
                                char const* label, ImFont* font, int id,
                                int flags, float2 size, bool enabled,
                                bool open) {
    ImGui::PushFont(font, 0.0f);
    utl::scope_guard guard = [] { ImGui::PopFont(); };
    ImGui::BeginDisabled(!enabled);
    auto& imgui_style = ImGui::GetStyle();
    float4 color =
        imgui_style.Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_Button];
    color.a *= style.alpha;
    float4 color_hovered =
        imgui_style
            .Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered];
    color_hovered.a *= style.alpha_hovered;
    float4 color_active = imgui_style.Colors[ImGuiCol_ButtonActive];
    color_active.a *= style.alpha_active;
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_active);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushID(id);
    bool result = ImGui::ButtonEx(label, size, flags);
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::EndDisabled();
    return result;
}

bool toolbar_button(ToolbarButtonStyle const& style, char const* label, int id,
                    int flags, float2 size, bool enabled = true,
                    bool open = false) {
    return toolbar_button_impl(style, label,
                               ui::font(FontWeight::semibold, FontStyle::roman),
                               id, flags, size, enabled, open);
}

bool toolbar_icon_button(ToolbarButtonStyle const& style, IconData icon, int id,
                         int flags, float2 size, char const* tooltip,
                         bool enabled = true, bool open = false) {
    bool result = toolbar_button_impl(style, icon.utf8_rep.data(), icon.font,
                                      id, flags, size, enabled, open);
    static float tooltip_timer = 1.0f;
    if (tooltip && ImGui::IsItemHovered()) {
        tooltip_timer -= ImGui::GetIO().DeltaTime;
        if (tooltip_timer <= 0.0f) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
    } else if (!ImGui::IsAnyItemHovered()) {
        tooltip_timer = 1.0f;
    }
    return result;
}

void ToolbarButton::Display::display(ToolbarButton const& button,
                                     ToolbarItemDisplayData data) {
    bool active =
        toolbar_button(data.button_style, button.m_label.c_str(), data.ID,
                       get_button_flags(button), data.actual_size);
    if (active) do_action(button);
}

float ToolbarButton::Display::calc_width(ToolbarButton const& button, float) {
    float2 text_size =
        ui::with_font(FontWeight::semibold, FontStyle::roman, [&] {
            return ImGui::CalcTextSize(button.m_label.data());
        });
    return text_size.x + 6 * ImGui::GetStyle().FramePadding.y;
}

ToolbarIconButton::ToolbarIconButton(Command const* command,
                                     std::string icon_name)
    : ToolbarIconButton(std::move(icon_name)) {
    if (command) {
        on_click([=] { command->execute(); });
        enabled([=] { return command->enabled(); });
        tooltip(command->name());
    } else {
        enabled(false);
    }
}

void ToolbarIconButton::Display::display(ToolbarIconButton const& button,
                                         ToolbarItemDisplayData data) {
    bool active = toolbar_icon_button(
        data.button_style, ui::icon_data({IconSize::_16}, button.m_icon_name()),
        data.ID, get_button_flags(button), data.actual_size, tooltip(button));
    if (active) do_action(button);
}

static float const icon_button_width_height_ratio = 1.7;

float ToolbarIconButton::Display::calc_width(ToolbarIconButton const&,
                                             float height) {
    return height * icon_button_width_height_ratio;
}

ToolbarDropdownMenu::ToolbarDropdownMenu()
    : m_string_id(utl::uuid::generate().to_string()) {}

bool ToolbarDropdownMenu::Display::begin_combo(
    ToolbarDropdownMenu const& item, ToolbarItemDisplayData const& data) {
    float2 cursor_pos_screen = ImGui::GetCursorScreenPos();
    ImRect bb(cursor_pos_screen, cursor_pos_screen + data.actual_size);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(item.m_string_id.data());
    ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
    bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    bool pressed = [&] {
        if (item.m_preview)
            return toolbar_button(data.button_style, item.m_preview().c_str(),
                                  data.ID, 0, data.actual_size, enabled(item),
                                  popup_open);
        char const* icon_name =
            item.m_icon_name ? item.m_icon_name() : "down-open";
        int flags = ImGuiButtonFlags_PressedOnClick |
                    ImGuiButtonFlags_PressedOnDragDropHold;
        return toolbar_icon_button(data.button_style,
                                   ui::icon_data({IconSize::_16}, icon_name),
                                   data.ID, flags, data.actual_size,
                                   tooltip(item), enabled(item), popup_open);
    }();
    if (pressed && !popup_open) {
        ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
        popup_open = true;
    }
    if (!popup_open) return false;
    ImGui::SetNextWindowSizeConstraints(item.m_min_size, {FLT_MAX, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        float2(2, 1) * (float2)ImGui::GetStyle().FramePadding);
    bool result = ImGui::BeginComboPopup(popup_id, bb, ImGuiComboFlags_None);
    ImGui::PopStyleVar();
    return result;
}

void ToolbarDropdownMenu::Display::display(ToolbarDropdownMenu const& item,
                                           ToolbarItemDisplayData const& data) {
    if (!begin_combo(item, data)) return;
    if (item.m_content) item.m_content();
    ImGui::EndCombo();
}

float ToolbarDropdownMenu::Display::calc_width(ToolbarDropdownMenu const& item,
                                               float height) {
    if (!item.m_preview) return height * icon_button_width_height_ratio;
    float2 text_size =
        ui::with_font(FontWeight::semibold, FontStyle::roman, [&] {
            return ImGui::CalcTextSize(item.m_preview().data());
        });
    return text_size.x + 6 * ImGui::GetStyle().FramePadding.y;
}

void ToolbarTextItem::Display::display(ToolbarTextItem const& item,
                                       ToolbarItemDisplayData const& data) {
    auto* font = ui::font(item.get_font_desc());
    auto* window = ImGui::GetCurrentWindow();
    auto* DL = window->DrawList;
    vml::float2 pos = ImGui::GetCursorScreenPos();
    auto& style = ImGui::GetStyle();
    pos.y += style.FramePadding.y;
    DL->AddText(font, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text),
                item.get_text());
    auto id = window->GetID(data.ID);
    ImRect bb(pos, pos + data.actual_size);
    ImGui::ItemAdd(bb, id);
    ImGui::ItemSize(bb);
}

float ToolbarTextItem::Display::calc_width(ToolbarTextItem const& item, float) {
    return ui::with_font(item.get_font_desc(), [&] {
        return ImGui::CalcTextSize(item.get_text()).x +
               2 * ImGui::GetStyle().FramePadding.x;
    });
}

char const* ToolbarTextItem::get_text() const { return m_text ? m_text() : ""; }

FontDesc ToolbarTextItem::get_font_desc() const {
    return m_font_desc ? m_font_desc() : FontDesc::ui_default();
}

void ToolbarEmptyItem::Display::display(ToolbarEmptyItem,
                                        ToolbarItemDisplayData const& data) {
    ImGui::Dummy(data.actual_size);
}

float ToolbarEmptyItem::Display::calc_width(ToolbarEmptyItem item, float) {
    return item.m_width;
}

void CustomToolbarElement::Display::display(
    CustomToolbarElement const& item, ToolbarItemDisplayData const& data) {
    if (item.m_content) item.m_content(data.actual_size);
}