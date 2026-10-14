#include "gui/toolbar.h"

#include <algorithm>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/uuid.hpp>
#include <utl/vector.hpp>

#include "gui/command_palette.h"
#include "gui/font.h"

using namespace mosaic;
using namespace vml::short_types;

static bool toolbar_button_impl(ToolbarStyle const& style, char const* label,
                                ImFont* font, int id, int flags, float2 size,
                                bool enabled, bool open) {
    ImGui::PushFont(font);
    utl::scope_guard guard = [] { ImGui::PopFont(); };
    ImGui::BeginDisabled(!enabled);
    auto& imgui_style = ImGui::GetStyle();
    float4 color =
        imgui_style.Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_Button];
    color.a *= style.button_alpha;
    float4 color_hovered =
        imgui_style
            .Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered];
    color_hovered.a *= style.button_alpha_hovered;
    float4 color_active = imgui_style.Colors[ImGuiCol_ButtonActive];
    color_active.a *= style.button_alpha_active;
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

bool toolbar_button(ToolbarStyle const& style, char const* label, int id,
                    int flags, float2 size, bool enabled = true,
                    bool open = false) {
    return toolbar_button_impl(style, label,
                               ui::font(FontWeight::semibold, FontStyle::roman),
                               id, flags, size, enabled, open);
}

bool toolbar_icon_button(ToolbarStyle const& style, IconData icon, int id,
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

void ToolbarButtonDisplay::display(ToolbarButton const& button,
                                   ToolbarDisplayData data) {
    bool active = toolbar_button(data.style, button.m_label.c_str(), data.ID,
                                 button.m_button_flags, data.actual_size);
    if (active && button.m_action) button.m_action();
}

float ToolbarButtonDisplay::calc_width(ToolbarButton const& button, float) {
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

void ToolbarIconButtonDisplay::display(ToolbarIconButton const& button,
                                       ToolbarDisplayData data) {
    bool active = toolbar_icon_button(
        data.style, ui::icon_data({IconSize::_16}, button.m_icon_name()),
        data.ID, button.m_button_flags, data.actual_size, tooltip(button));
    if (active && button.m_action) button.m_action();
}

static float const icon_button_width_height_ratio = 1.7;

float ToolbarIconButtonDisplay::calc_width(ToolbarIconButton const&,
                                           float height) {
    return height * icon_button_width_height_ratio;
}

ToolbarDropdownMenu::ToolbarDropdownMenu()
    : m_string_id(utl::uuid::generate().to_string()) {}

bool ToolbarDropdownMenu::Display::begin_combo(ToolbarDropdownMenu const& item,
                                               ToolbarDisplayData const& data) {
    float2 cursor_pos_screen = ImGui::GetCursorScreenPos();
    float2 size = {data.actual_size.x,
                   data.actual_size.y + ImGui::GetStyle().ItemSpacing.y};
    ImRect bb(cursor_pos_screen, cursor_pos_screen + size);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiID id = window->GetID(item.m_string_id.data());
    ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
    bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    bool pressed = [&] {
        if (item.m_preview)
            return toolbar_button(data.style, item.m_preview().c_str(), data.ID,
                                  0, size, enabled(item), popup_open);
        char const* icon_name =
            item.m_icon_name ? item.m_icon_name() : "down-open";
        int flags = ImGuiButtonFlags_PressedOnClick |
                    ImGuiButtonFlags_PressedOnDragDropHold;
        return toolbar_icon_button(
            data.style, ui::icon_data({IconSize::_16}, icon_name), data.ID,
            flags, size, tooltip(item), enabled(item), popup_open);
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
                                           ToolbarDisplayData const& data) {
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
                                       ToolbarDisplayData const&) {
    auto* font = ui::font(item.get_font_desc());
    auto* DL = ImGui::GetWindowDrawList();
    float2 pos = ImGui::GetCursorScreenPos();
    auto& style = ImGui::GetStyle();
    pos.y += style.FramePadding.y;
    DL->AddText(font, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text),
                item.get_text());
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
                                        ToolbarDisplayData const& data) {
    ImGui::Dummy(data.actual_size);
}

float ToolbarEmptyItem::Display::calc_width(ToolbarEmptyItem item, float) {
    return item.m_width;
}

float Toolbar::get_width_without_spacers() const {
    return std::accumulate(blocks.begin(), blocks.end(), 0.0f,
                           [](float acc, Block const& block) {
                               return acc + block.width;
                           }) +
           std::max(0.0f, (float)blocks.size() - 1.0f) *
               GImGui->Style.ItemSpacing.x;
}

void Toolbar::display(std::optional<float> width) noexcept {
    float2 padding = get_padding();
    float actual_height =
        std::min(ImGui::GetContentRegionAvail().y, style.height);
    /// We actually cook every frame because performance is really not an issue
    /// and it solves some caching issues
    cook(actual_height);
    if (!width) width = ImGui::GetContentRegionAvail().x;
    *width -= 2 * padding.x;
    float2 position = ImGui::GetCursorPos();
    /// Display blocks
    calc_offsets(*width);
    for (auto const& block : blocks) {
        if (block.visible) {
            ImGui::SetCursorPos(position + padding + float2(block.offset, 0));
            display_block(block, actual_height);
        }
    }
    float height_with_padding = actual_height + 2 * padding.y;
    ImGui::SetCursorPos(position + float2(0, height_with_padding));
    /// Draw separator
    if (!(style.flags & ToolbarFlags::no_decoration)) {
        auto* window = ImGui::GetCurrentWindowRead();
        float2 pos =
            (float2)ImGui::GetWindowPos() + float2(0, window->TitleBarHeight);
        float2 from = pos + float2(0, height_with_padding);
        float2 to = from + float2(ImGui::GetWindowWidth(), 0);
        auto* DL = ImGui::GetWindowDrawList();
        DL->PushClipRect(window->Rect().Min, window->Rect().Max);
        DL->AddLine(from, to, ImGui::GetColorU32(ImGuiCol_Separator), 1);
        DL->PopClipRect();
    }
}

void Toolbar::display_block(Block const& block, float actual_height) {
    float cursor_pos_y = ImGui::GetCursorPos().y;
    for (size_t i = block.item_begin_index; i < block.item_end_index; ++i) {
        auto* item = all_items[i].get();
        ImGui::BeginDisabled(!item->enabled());
        item->display({.style = style,
                       .actual_size = {item->width(), actual_height},
                       .ID = item->ID()});
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosY(cursor_pos_y);
    }
}

void Toolbar::cook(float actual_height) {
    blocks.clear();
    if (all_items.empty()) return;
    /// Partition into blocks
    blocks.emplace_back();
    blocks.back().item_begin_index = 0;
    for (size_t index = 0; auto& item : all_items) {
        if (item->is_spacer()) {
            blocks.back().item_end_index = index + 1;
            blocks.emplace_back();
            blocks.back().item_begin_index = index + 1;
        }
        ++index;
    }
    blocks.back().item_end_index = all_items.size();
    /// Calculate item widths
    for (auto& item : all_items)
        item->calc_width(actual_height);
    /// Calculate block widths
    for (auto& block : blocks) {
        float acc_items_width = std::accumulate(
            all_items.begin() + block.item_begin_index,
            all_items.begin() + block.item_end_index, 0.0f,
            [](float acc, auto& item) { return acc + item->width(); });
        int num_items = utl::narrow_cast<int>(block.item_end_index -
                                              block.item_begin_index);
        float spacing =
            std::max(0, num_items - 1) * ImGui::GetStyle().ItemSpacing.x;
        block.width = acc_items_width + spacing;
    }
}

void Toolbar::calc_offsets(float total_width) {
    if (blocks.size() == 0) return;
    for (auto& block : blocks)
        block.visible = false;
    float cursor = 0;
    float width_avail = total_width;
    blocks[0].offset = cursor;
    blocks[0].visible = true;
    if (blocks.size() == 1) return;
    cursor += blocks[0].width;
    width_avail -= blocks[0].width;
    auto& last_block = blocks.back();
    if (last_block.width > width_avail) return;
    last_block.visible = true;
    last_block.offset = total_width - last_block.width;
    width_avail -= last_block.width;
    if (blocks.size() == 2) return;
    if (blocks[1].width > width_avail) return;
    blocks[1].visible = true;
    blocks[1].offset = cursor + (width_avail - blocks[1].width) / 2;
}

float Toolbar::get_height() const { return style.height + 2 * get_padding().y; }

float2 Toolbar::get_padding() const {
    auto& style = ImGui::GetStyle();
    auto padding = this->style.padding;
    return {
        padding.x < 0.0f ? style.WindowPadding.x : padding.x,
        padding.y < 0.0f ? style.WindowPadding.y : padding.y,
    };
}
