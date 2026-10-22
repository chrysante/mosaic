#include "gui/toolbar_renderer.h"

#include <algorithm>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/utility.hpp>

#include "application/debug_settings.h"
#include "gui/toolbar.h"

using namespace mosaic;

ToolbarPadding mosaic::resolve_toolbar_padding(vml::float2 in) {
    auto value_or = [](float value, float fallback) {
        return value >= 0.0f ? value : fallback;
    };
    auto& style = ImGui::GetStyle();
    float x = value_or(in.x, style.WindowPadding.x);
    float y = value_or(in.y, style.WindowPadding.y);
    return {
        .top = y,
        .bottom = y,
        .left = x,
        .right = x,
    };
}

#ifndef NDEBUG
static void debug_draw_geometry(ToolbarGeometry geometry) {
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    vml::float2 pos = (vml::float2)ImGui::GetWindowPos() + geometry.position;
    vml::float2 max = pos + geometry.size;
    vml::float2 paddedmin = pos + geometry.padding.top_left();
    vml::float2 paddedmax = max - geometry.padding.bottom_right();
    // Padded bounds
    draw_list->AddRect(paddedmin, paddedmax, IM_COL32(255, 180, 0, 220), 0.0f,
                       1.5f);
    // Toolbar bounds
    draw_list->AddRect(pos, max, IM_COL32(0, 220, 120, 255), 0.0f, 2.0f);
}
#endif

float ToolbarRenderer::get_width_without_spacers(ToolbarStyle const& style,
                                                 ToolbarPadding padding) const {
    float raw_width = std::accumulate(
        m_blocks.begin(), m_blocks.end(), 0.0f,
        [](float acc, Block const& block) { return acc + block.width; });
    float spacing =
        std::max(0.0f, (float)m_blocks.size() - 1.0f) * style.item_spacing;
    return raw_width + spacing + padding.total_x();
}

static void draw_separator(ToolbarGeometry geometry, float thickness) {
    auto* window = ImGui::GetCurrentWindowRead();
    vml::float2 pos = (vml::float2)ImGui::GetWindowPos() +
                      vml::float2(0, window->TitleBarHeight);
    vml::float2 from = pos + vml::float2(0, geometry.size.y);
    vml::float2 to = from + vml::float2(ImGui::GetWindowWidth(), 0);
    auto* DL = ImGui::GetWindowDrawList();
    DL->PushClipRect(window->Rect().Min, window->Rect().Max);
    DL->AddLine(from, to, ImGui::GetColorU32(ImGuiCol_Separator), thickness);
    DL->PopClipRect();
}

void ToolbarRenderer::display(ToolbarStyle const& style,
                              ToolbarGeometry geometry) {
    float item_height = geometry.size.y - geometry.padding.total_y();
    // We actually cook every frame because performance is really not an issue
    // and it solves some caching issues
    cook(style, item_height);
    float item_space_x = geometry.size.x - geometry.padding.total_x();
    calc_offsets(item_space_x);
    display_blocks(style, geometry.position, item_height, geometry.padding);
    float pos_y = geometry.position.y + geometry.size.y;
    if (!(style.flags & ToolbarFlags::no_decoration)) {
        float thickness = ImGui::GetStyle().SeparatorSize;
        draw_separator(geometry, thickness);
        pos_y += thickness;
    };
    ImGui::SetCursorScreenPos({geometry.position.x, pos_y});
    ImGui::Dummy({0.0f, 0.0f});
#ifndef NDEBUG
    if (want_debug_overlays()) debug_draw_geometry(geometry);
#endif
}

static void display_ending_spacer(ToolbarStyle const& style,
                                  ToolbarItem const* spacer,
                                  vml::float2 cursor_pos,
                                  vml::float2 next_cursor_pos,
                                  float item_height) {
    if (!spacer) return;
    assert(spacer->is_spacer());
    float spacer_width =
        next_cursor_pos.x - cursor_pos.x - 2 * style.item_spacing;
    if (spacer_width >= spacer->width()) {
        ImGui::SetCursorPos({cursor_pos.x + style.item_spacing, cursor_pos.y});
        spacer->display({.button_style = style.button,
                         .actual_size = {spacer_width, item_height},
                         .ID = spacer->ID()});
    }
}

void ToolbarRenderer::display_blocks(ToolbarStyle const& style,
                                     vml::float2 position, float item_height,
                                     ToolbarPadding padding) {
    for (std::size_t i = 0; i < m_blocks.size(); ++i) {
        auto const& block = m_blocks[i];
        vml::float2 cursor_pos = {position.x + padding.left + block.offset,
                                  position.y + padding.top};
        if (block.visible) display_block(style, block, cursor_pos, item_height);
        if (i == m_blocks.size() - 1) continue;
        cursor_pos.x += block.width;
        vml::float2 next_cursor_pos = {position.x + padding.left +
                                           m_blocks[i + 1].offset,
                                       position.y + padding.top};
        display_ending_spacer(style, block.ending_spacer, cursor_pos,
                              next_cursor_pos, item_height);
    }
}

void ToolbarRenderer::display_block(ToolbarStyle const& style,
                                    Block const& block, vml::float2 cursor_pos,
                                    float item_height) {
    for (size_t i = block.item_begin_index; i < block.item_end_index; ++i) {
        auto& item = m_toolbar->item_at(i);
        ImGui::BeginDisabled(!item.enabled());
        ImGui::SetCursorPos(cursor_pos);
        vml::float2 item_size = {item.width(), item_height};
        item.display({.button_style = style.button,
                      .actual_size = item_size,
                      .ID = item.ID()});
        ImGui::EndDisabled();
        cursor_pos.x += item_size.x + style.item_spacing;
    }
}

void ToolbarRenderer::cook(ToolbarStyle const& style, float item_height) {
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
        float spacing = std::max(0, num_items - 1) * style.item_spacing;
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
    m_blocks[0].visible = total_width > 0.0f;
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