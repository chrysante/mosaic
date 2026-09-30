#include "gui/toolbar.h"

#include <algorithm>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/uuid.hpp>
#include <utl/vector.hpp>

#include "gui/command_palette.h"
#include "gui/font.h"

using namespace mosaic;
using namespace vml::short_types;

static float2 compute_padding(std::optional<float2> padding) {
    return padding.value_or(ImGui::GetStyle().WindowPadding);
}

static bool tb_button_impl(ToolbarStyle const& style, char const* label, int id,
                           int flags, float2 size, bool enabled, bool open) {
    ImGui::BeginDisabled(!enabled);
    float4 color =
        GImGui->Style.Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_Button];
    color.a *= style.button_alpha;
    float4 color_hovered =
        GImGui->Style
            .Colors[open ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered];
    color_hovered.a *= style.button_alpha_hovered;
    float4 color_active = GImGui->Style.Colors[ImGuiCol_ButtonActive];
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

bool tb_button(ToolbarStyle const& style, char const* label, int id, int flags,
               float2 size, bool enabled = true, bool open = false) {
    return ui::with_font(FontWeight::semibold, FontStyle::roman, [&] {
        return tb_button_impl(style, label, id, flags, size, enabled, open);
    });
}

bool tb_icon_button(ToolbarStyle const& style, IconData icon, int id, int flags,
                    float2 size, char const* tooltip, bool enabled = true,
                    bool open = false) {
    bool result = ui::with_font(icon.font, [&] {
        return tb_button_impl(style, icon.utf8_rep.data(), id, flags, size,
                              enabled, open);
    });
    static float tooltip_timer = 1.0f;
    if (tooltip && ImGui::IsItemHovered()) {
        tooltip_timer -= GImGui->IO.DeltaTime;
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

template <typename Derived>
void ToolbarButtonMixin<Derived>::do_action() const {
    if (m_action) m_action();
}

void ToolbarButton::display(ToolbarStyle const& style) {
    if (tb_button(style, m_label.c_str(), id(), m_button_flags,
                  float2(get_width(), style.actual_height))) {
        do_action();
    }
}

float ToolbarButton::calc_width(float /* height */) const {
    float2 text_size =
        ui::with_font(FontWeight::semibold, FontStyle::roman,
                      [&] { return ImGui::CalcTextSize(m_label.data()); });
    return text_size.x + 6 * GImGui->Style.FramePadding.y;
}

ToolbarIconButton::ToolbarIconButton(Command const* command,
                                     std::string icon_name)
    : ToolbarIconButton(std::move(icon_name)) {
    if (!command) {
        enabled(false);
        return;
    }
    on_click([=] { command->execute(); });
    enabled([=] { return command->enabled(); });
    tooltip(command->name());
}

void ToolbarIconButton::display(ToolbarStyle const& style) {
    if (tb_icon_button(style, ui::icon_data({IconSize::_16}, m_icon()), id(),
                       m_button_flags, float2(get_width(), style.actual_height),
                       get_tooltip())) {
        do_action();
    }
}

static float const icon_button_width_height_ratio = 1.7;

float ToolbarIconButton::calc_width(float height) const {
    return height * icon_button_width_height_ratio;
}

ToolbarDropdownMenu::ToolbarDropdownMenu()
    : m_id(utl::uuid::generate().to_string()) {}

bool ToolbarDropdownMenu::begin_combo(ToolbarStyle const& style) {
    float2 size = {get_width(), style.actual_height};
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImRect bb(
        window->DC.CursorPos,
        (float2)window->DC.CursorPos +
            float2(size.x, style.actual_height + GImGui->Style.ItemSpacing.y));
    ImGuiID id = window->GetID(m_id.data());
    ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
    bool popup_open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    bool pressed = false;
    if (m_preview) {
        pressed = tb_button(style, m_preview().c_str(), this->id(), 0, size,
                            is_enabled(), popup_open);
    } else {
        char const* icon = m_icon ? m_icon() : "down-open";
        int flags = ImGuiButtonFlags_PressedOnClick |
                    ImGuiButtonFlags_PressedOnDragDropHold;
        pressed = tb_icon_button(style, ui::icon_data({IconSize::_16}, icon),
                                 this->id(), flags, size, get_tooltip(),
                                 is_enabled(), popup_open);
    }
    if (pressed && !popup_open) {
        ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
        popup_open = true;
    }
    if (!popup_open) return false;
    ImGui::SetNextWindowSizeConstraints(m_min_size, {FLT_MAX, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        float2(2, 1) * (float2)ImGui::GetStyle().FramePadding);
    bool result = ImGui::BeginComboPopup(popup_id, bb, ImGuiComboFlags_None);
    ImGui::PopStyleVar();
    return result;
}

void ToolbarDropdownMenu::display(ToolbarStyle const& style) {
    if (begin_combo(style)) {
        if (m_content) m_content();
        ImGui::EndCombo();
    }
}

float ToolbarDropdownMenu::calc_width(float height) const {
    if (!m_preview) return height * icon_button_width_height_ratio;
    float2 text_size =
        ui::with_font(FontWeight::semibold, FontStyle::roman,
                      [&] { return ImGui::CalcTextSize(m_preview().data()); });
    return text_size.x + 6 * GImGui->Style.FramePadding.y;
}

void ToolbarSeparator::display(ToolbarStyle const&) {}

float ToolbarSeparator::calc_width(float /* height */) const {
    return 0.0f;
}

void ToolbarSpacer::display(ToolbarStyle const&) {}

float ToolbarSpacer::calc_width(float /* height */) const {
    return 0.0f;
}

void ToolbarTextItem::display(ToolbarStyle const&) {
    auto* font = ui::font(get_font_desc());
    auto* DL = ImGui::GetWindowDrawList();
    float2 pos = ImGui::GetCursorScreenPos();
    auto& style = ImGui::GetStyle();
    pos.y += style.FramePadding.y;
    DL->AddText(font, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text), get_text());
}

float ToolbarTextItem::calc_width([[maybe_unused]] float height) const {
    return ui::with_font(get_font_desc(), [&] {
        return ImGui::CalcTextSize(get_text()).x +
               2 * ImGui::GetStyle().FramePadding.x;
    });
}

char const* ToolbarTextItem::get_text() const {
    return m_text ? m_text() : "";
}

FontDesc ToolbarTextItem::get_font_desc() const {
    return m_font_desc ? m_font_desc() : FontDesc::ui_default();
}

void ToolbarEmptyItem::display(ToolbarStyle const& style) {
    ImGui::Dummy(float2(get_width(), style.actual_height));
}

float ToolbarEmptyItem::calc_width([[maybe_unused]] float height) const {
    return m_width;
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
    float new_height = std::min(ImGui::GetContentRegionAvail().y, style.height);
    style.actual_height = new_height;
    /// We actually cook every frame because performance is really not an issue
    /// and it solves some caching issues
    cook();
    if (!width) width = ImGui::GetContentRegionAvail().x;
    *width -= 2 * padding.x;
    float2 position = ImGui::GetCursorPos();
    /// Display blocks
    calc_offsets(*width);
    for (auto const& block : blocks) {
        if (block.visible) {
            ImGui::SetCursorPos(position + padding + float2(block.offset, 0));
            display_block(block);
        }
    }
    float height_with_padding = style.actual_height + 2 * padding.y;
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

void Toolbar::display_block(Block const& block) {
    float y = ImGui::GetCursorPos().y;
    for (size_t i = block.item_begin_index; i < block.item_end_index; ++i) {
        bool enabled = all_items[i]->is_enabled();
        ImGui::BeginDisabled(!enabled);
        all_items[i]->display(style);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosY(y);
    }
}

void Toolbar::cook() {
    blocks.clear();
    if (all_items.empty()) return;
    /// Partition into blocks
    blocks.emplace_back();
    blocks.back().item_begin_index = 0;
    for (size_t index = 0; auto& item : all_items) {
        if (dynamic_cast<ToolbarSpacer const*>(item.get())) {
            blocks.back().item_end_index = index + 1;
            blocks.emplace_back();
            blocks.back().item_begin_index = index + 1;
        }
        ++index;
    }
    blocks.back().item_end_index = all_items.size();
    /// Calculate item widths
    for (auto& item : all_items)
        item->m_width = item->calc_width(style.actual_height);
    /// Calculate block widths
    for (auto& block : blocks) {
        float acc_items_width = std::accumulate(
            all_items.begin() + block.item_begin_index,
            all_items.begin() + block.item_end_index, 0.0f,
            [](float acc, auto& item) { return acc + item->get_width(); });
        int num_items = utl::narrow_cast<int>(block.item_end_index -
                                              block.item_begin_index);
        float spacing =
            std::max(0, num_items - 1) * GImGui->Style.ItemSpacing.x;
        block.width = acc_items_width + spacing;
    }
}

void Toolbar::calc_offsets(float totalWidth) {
    if (blocks.size() == 0) return;
    for (auto& block : blocks)
        block.visible = false;
    float cursor = 0;
    float width_avail = totalWidth;
    blocks[0].offset = cursor;
    blocks[0].visible = true;
    if (blocks.size() == 1) return;
    cursor += blocks[0].width;
    width_avail -= blocks[0].width;
    auto& last_block = blocks.back();
    if (last_block.width > width_avail) return;
    last_block.visible = true;
    last_block.offset = totalWidth - last_block.width;
    width_avail -= last_block.width;
    if (blocks.size() == 2) return;
    if (blocks[1].width > width_avail) return;
    blocks[1].visible = true;
    blocks[1].offset = cursor + (width_avail - blocks[1].width) / 2;
}

float Toolbar::get_height() const {
    return style.height + 2 * get_padding().y;
}

float2 Toolbar::get_padding() const {
    return compute_padding(style.padding);
}
