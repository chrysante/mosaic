#include "gui/toolbar_renderer.h"

#include <algorithm>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/utility.hpp>

#include "application/debug_settings.h"
#include "gui/toolbar.h"

using namespace mosaic;

namespace {

struct ToolbarGeometry {
    vml::float2 position;
    vml::float2 size;
    ToolbarPadding padding;
};

} // namespace

static void debug_draw_geometry(ToolbarGeometry geometry) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    vml::float2 max = geometry.position + geometry.size;
    vml::float2 paddedmin =
        geometry.position +
        vml::float2{geometry.padding.left, geometry.padding.top};
    vml::float2 paddedmax =
        max - vml::float2{geometry.padding.right, geometry.padding.bottom};
    // Padded bounds
    draw_list->AddRect(paddedmin, paddedmax, IM_COL32(255, 180, 0, 220), 0.0f,
                       0, 1.5f);
    // Toolbar bounds
    draw_list->AddRect(geometry.position, max, IM_COL32(0, 220, 120, 255), 0.0f,
                       0, 2.0f);
}

static ToolbarPadding resolve_padding(ToolbarPadding in) {
    auto value_or = [](float value, float fallback) {
        return value >= 0.0f ? value : fallback;
    };
    auto& style = ImGui::GetStyle();
    return {
        .top = value_or(in.top, style.WindowPadding.y),
        .bottom = value_or(in.bottom, style.WindowPadding.y),
        .left = value_or(in.left, style.WindowPadding.x),
        .right = value_or(in.right, style.WindowPadding.x),
    };
}

float ToolbarRenderer::get_width_without_spacers() const {
    float raw_width = std::accumulate(
        m_blocks.begin(), m_blocks.end(), 0.0f,
        [](float acc, Block const& block) { return acc + block.width; });
    float spacing = std::max(0.0f, (float)m_blocks.size() - 1.0f) *
                    ImGui::GetStyle().ItemSpacing.x;
    ToolbarPadding padding = resolve_padding(m_toolbar->style().padding);
    return raw_width + spacing + padding.left + padding.right;
}

void ToolbarRenderer::display(vml::float2 size) {
    ToolbarPadding padding = resolve_padding(m_toolbar->style().padding);
    float item_height = size.y - padding.top - padding.bottom;
    /// We actually cook every frame because performance is really not an issue
    /// and it solves some caching issues
    cook(item_height);
    float item_space_y = size.x - padding.left - padding.right;
    calc_offsets(item_space_y);
    vml::float2 position = ImGui::GetCursorPos();
    display_blocks(position, item_height, padding);
    ImGui::SetCursorPos(position + vml::float2(0, size.y));
    /// Draw separator
    if (!(m_toolbar->style().flags & ToolbarFlags::no_decoration)) {
        auto* window = ImGui::GetCurrentWindowRead();
        vml::float2 pos = (vml::float2)ImGui::GetWindowPos() +
                          vml::float2(0, window->TitleBarHeight);
        vml::float2 from = pos + vml::float2(0, size.y);
        vml::float2 to = from + vml::float2(ImGui::GetWindowWidth(), 0);
        auto* DL = ImGui::GetWindowDrawList();
        DL->PushClipRect(window->Rect().Min, window->Rect().Max);
        DL->AddLine(from, to, ImGui::GetColorU32(ImGuiCol_Separator), 1);
        DL->PopClipRect();
    }
    if (want_debug_overlays())
        debug_draw_geometry(
            {.position = (vml::float2)ImGui::GetWindowPos() + position,
             .size = size,
             .padding = padding});
}

void ToolbarRenderer::display_blocks(vml::float2 position, float item_height,
                                     ToolbarPadding padding) {
    for (std::size_t i = 0; i < m_blocks.size(); ++i) {
        auto const& block = m_blocks[i];
        vml::float2 cursor_pos = {position.x + padding.left + block.offset,
                                  position.y + padding.top};
        auto* last_spacer = [&] -> ToolbarItem const* {
            if (i == 0) return nullptr;
            return m_blocks[i - 1].ending_spacer;
        }();
        if (last_spacer) {
            vml::float2 last_cursor_pos = ImGui::GetCursorPos();
            float spacer_width = cursor_pos.x - last_cursor_pos.x -
                                 2 * m_toolbar->style().item_spacing;
            ImGui::SetCursorPos({last_cursor_pos.x, cursor_pos.y});
            last_spacer->display({.button_style = m_toolbar->style().button,
                                  .actual_size = {spacer_width, item_height},
                                  .ID = last_spacer->ID()});
        }
        ImGui::SetCursorPos(cursor_pos);
        display_block(block, item_height);
    }
}

void ToolbarRenderer::display_block(Block const& block, float item_height) {
    float cursor_pos_y = ImGui::GetCursorPos().y;
    for (size_t i = block.item_begin_index; i < block.item_end_index; ++i) {
        auto& item = m_toolbar->item_at(i);
        ImGui::BeginDisabled(!item.enabled());
        item.display({.button_style = m_toolbar->style().button,
                      .actual_size = {item.width(), item_height},
                      .ID = item.ID()});
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosY(cursor_pos_y);
    }
}

void ToolbarRenderer::cook(float item_height) {
    m_blocks.clear();
    if (m_toolbar->num_items() == 0) return;
    /// Partition into blocks
    m_blocks.emplace_back();
    m_blocks.back().item_begin_index = 0;
    std::size_t num_items = m_toolbar->num_items();
    for (size_t index = 0; index < num_items; ++index) {
        auto& item = m_toolbar->item_at(index);
        const_cast<ToolbarItem&>(item).calc_width(item_height);
        if (item.is_spacer()) {
            m_blocks.back().item_end_index = index;
            m_blocks.back().ending_spacer = &item;
            m_blocks.emplace_back();
            m_blocks.back().item_begin_index = index + 1;
        }
    }
    m_blocks.back().item_end_index = num_items;
    /// Calculate block widths
    for (auto& block : m_blocks) {
        float acc_items_width = [&] {
            float acc = 0.0f;
            for (std::size_t i = block.item_begin_index;
                 i < block.item_end_index; ++i)
                acc += m_toolbar->item_at(i).width();
            return acc;
        }();
        int num_items = utl::narrow_cast<int>(block.item_end_index -
                                              block.item_begin_index);
        float spacing =
            std::max(0, num_items - 1) * m_toolbar->style().item_spacing;
        block.width = acc_items_width + spacing;
    }
}

void ToolbarRenderer::calc_offsets(float total_width) {
    if (m_blocks.size() == 0) return;
    for (auto& block : m_blocks)
        block.visible = false;
    float cursor = 0;
    float width_avail = total_width;
    m_blocks[0].offset = cursor;
    m_blocks[0].visible = true;
    if (m_blocks.size() == 1) return;
    cursor += m_blocks[0].width;
    width_avail -= m_blocks[0].width;
    auto& last_block = m_blocks.back();
    if (last_block.width > width_avail) return;
    last_block.visible = true;
    last_block.offset = total_width - last_block.width;
    width_avail -= last_block.width;
    if (m_blocks.size() == 2) return;
    if (m_blocks[1].width > width_avail) return;
    m_blocks[1].visible = true;
    m_blocks[1].offset = cursor + (width_avail - m_blocks[1].width) / 2;
}