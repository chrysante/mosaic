#include "gui/containers.h"

#include <algorithm>
#include <cstdio>
#include <mach/memory_object_types.h>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/common.hpp>
#include <utl/scope_guard.hpp>

#include "application/debug_settings.h"
#include "gui/view_debugger.h"
#include "gui/view_support.h"
#include "support/logger.h"

using namespace mosaic;
using namespace ui;

static float resolve_child_size(SizeReq requirement, float intrinsic,
                                float available) {
    auto clamp = [available](float value) {
        return available >= 0 ? std::min(value, available) : value;
    };

    switch (requirement.type()) {
    case SizeReq::Type::fixed:
        return clamp(requirement.fixed_size());
    case SizeReq::Type::fit:
        return clamp(intrinsic);
    case SizeReq::Type::fill:
        return clamp(available >= 0 ? available : intrinsic);
    }
}

StackView::StackView(Axis main_axis, ViewContainer<>&& children,
                     std::optional<Alignment> align_override)
    : View(default_size_requirement)
    , m_main_axis(main_axis)
    , m_align_override(align_override)
    , m_children(std::move(children)) {}

using OverflowStrategy = void (*)(std::span<vml::float2> sizes,
                                  std::size_t main_axis, float overflow);

[[maybe_unused]]
static void shrink_to_fit_equally(std::span<vml::float2> sizes,
                                  std::size_t main_axis, float overflow) {
    if (sizes.empty() || overflow <= 0.0f) return;
    utl::small_vector<unsigned, 32> indices(sizes.size(), utl::no_init);
    for (unsigned i = 0; i < sizes.size(); ++i) {
        assert(sizes[i][main_axis] >= 0.0f);
        indices[i] = i;
    }
    auto main_axis_less = [sizes, main_axis](unsigned i, unsigned j) {
        return sizes[i][main_axis] < sizes[j][main_axis];
    };
    std::sort(indices.begin(), indices.end(), main_axis_less);
    std::size_t num_remaining = sizes.size();
    for (unsigned i : indices) {
        if (overflow <= 0.0f) break;
        float share = overflow / static_cast<float>(num_remaining);
        --num_remaining;
        float& size = sizes[i][main_axis];
        float reduction = std::min(size, share);
        size -= reduction;
        overflow -= reduction;
    }
}

static void shrink_to_fit_proportionally(std::span<vml::float2> sizes,
                                         std::size_t main_axis,
                                         float overflow) {
    if (sizes.empty() || overflow <= 0.0f) return;
    float total_size = 0.0f;
    for (auto const& size : sizes) {
        assert(size[main_axis] >= 0.0f);
        total_size += size[main_axis];
    }
    if (total_size <= 0.0f) return;
    float const remaining_size = std::max(0.0f, total_size - overflow);
    float const scale = remaining_size / total_size;
    for (auto& size : sizes)
        size[main_axis] *= scale;
}

#ifndef NDEBUG
ImU32 const color_warning = IM_COL32(255, 128, 0, 255);
ImU32 const color_error = IM_COL32(255, 0, 0, 255);

static void draw_overflow_debug_overlay(Rect child_frame, unsigned main_axis) {
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddRectFilled(child_frame.min(), child_frame.max(),
                      IM_COL32(0, 0, 0, 255));
    ImGui::PushClipRect(child_frame.min(), child_frame.max(), true);
    char buf[256];
    std::snprintf(buf, sizeof buf, "OVERFLOW:\nResolved Size:\n(%.0f, %.0f)",
                  child_frame.size.x, child_frame.size.y);
    DL->AddText(child_frame.min(), color_warning, buf);
    ImGui::PopClipRect();
    if (child_frame.size[main_axis] > 1.0f)
        DL->AddRect(child_frame.min(), child_frame.max(), color_warning);
    else
        DL->AddRect(child_frame.min(), child_frame.max(), color_error, 0.0f,
                    4.0f);
}

static void overflow_debug_info() {
    debug_node_info([] {
        ImGui::PushStyleColor(ImGuiCol_Text, color_warning);
        ImGui::Text("Have overflow.");
        ImGui::Text("Strategy: shrink_to_fit_proportionally");
        ImGui::PopStyleColor();
    });
}

static void overflow_debug_info_child(View const* child, Rect child_frame,
                                      unsigned main_axis) {
    (void)child;
    bool const hovered = ImGui::IsItemHovered();
    if (want_debug_overlays())
        draw_overflow_debug_overlay(child_frame, main_axis);
    debug_node_info([=] {
        ImGui::Indent();
        ImU32 const col = hovered ? IM_COL32(31, 128, 200, 255) : color_warning;
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("Resolved Size: (%.0f, %.0f)", child_frame.size.x,
                    child_frame.size.y);
        ImGui::PopStyleColor();
        ImGui::Unindent();
    });
}
#endif

static float size_one_less(std::size_t size) {
    return size == 0 ? 0.0f : (float)(size - 1);
}

void StackView::do_display(Rect frame) {
    if (m_children.empty()) return;

    const unsigned main_axis = (unsigned)m_main_axis;
    const unsigned cross_axis = 1 - main_axis;
    const vml::float2 size_avail = frame.size;

    // Spacing must not consume more space than the entire main-axis frame.
    // Normally this leaves ImGui's spacing unchanged.
    float spacing = ImGui::GetStyle().ItemSpacing[main_axis];
    const float available_main = std::max(0.0f, size_avail[main_axis]);
    const float num_gaps = size_one_less(m_children.size());

    if (num_gaps > 0.0f) spacing = std::min(spacing, available_main / num_gaps);

    const float total_spacing = spacing * num_gaps;

    // Measure intrinsic sizes and calculate each child's initial allocation.
    utl::small_vector<vml::float2, 32> child_sizes(m_children.size(),
                                                   utl::no_init);
    float desired_main_sum = 0.0f;
    float fill_weight_sum = 0.0f;
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        View& child = m_children[i];
        vml::float2 child_max_size = frame.size;
        child_max_size[main_axis] = -1.0f;
        vml::float2 const intrinsic_size = child_sizes[i] =
            child.calc_intrinsic_size(child_max_size);
        SizeReq2 const requirement = child.size_requirement();
        SizeReq const main_requirement = requirement[main_axis];

        // Resolve the desired size without allocating any extra fill space.
        // For fill requirements this gives the child's intrinsic base size.
        float const desired_main = resolve_child_size(
            main_requirement, intrinsic_size[main_axis], -1.0f);
        assert(desired_main >= 0.0f);
        child_sizes[i][main_axis] = desired_main;
        desired_main_sum += desired_main;
        if (main_requirement.is_fill())
            fill_weight_sum += main_requirement.fill_weight();
    }

    // Determine whether the initial allocations fit.
    float const remaining = available_main - desired_main_sum - total_spacing;
    bool const have_overflow = remaining < 0.0f;

    if (have_overflow) {
        // Keep the strategy swappable: a future stack option can select
        // another strategy here, e.g. priority-based child eviction.
        constexpr OverflowStrategy overflow_strategy =
            shrink_to_fit_proportionally;
        overflow_strategy(child_sizes, main_axis, -remaining);
#ifndef NDEBUG
        overflow_debug_info();
#endif
    } else if (remaining > 0.0f && fill_weight_sum > 0.0f) {
        // Only distribute surplus space when the initial allocations fit.
        for (std::size_t i = 0; i < m_children.size(); ++i) {
            SizeReq2 const requirement = m_children[i].size_requirement();
            SizeReq const main_requirement = requirement[main_axis];
            if (!main_requirement.is_fill()) continue;
            float const fill_share =
                main_requirement.fill_weight() / fill_weight_sum;
            child_sizes[i][main_axis] += remaining * fill_share;
        }
    }

    // Place children using their final main-axis allocations. Cross-axis sizing
    // and alignment continue to follow the existing size requirements.
    vml::float2 cursor = frame.origin;

    for (std::size_t i = 0; i < m_children.size(); ++i) {
        auto& child = m_children[i];
        vml::float2 child_size = child_sizes[i];
        SizeReq2 const size_req = child.size_requirement();
        child_size[cross_axis] =
            resolve_child_size(size_req[cross_axis], child_size[cross_axis],
                               size_avail[cross_axis]);
        Alignment const alignment = m_align_override.value_or(
            child.alignment_requirement()[cross_axis]);
        float cross_offset = 0.0f;
        switch (alignment) {
        case Alignment::low:
            cross_offset = 0.0f;
            break;
        case Alignment::mid:
            cross_offset =
                0.5f * (frame.size[cross_axis] - child_size[cross_axis]);
            break;
        case Alignment::high:
            cross_offset = frame.size[cross_axis] - child_size[cross_axis];
            break;
        }
        vml::float2 pos = cursor;
        pos[cross_axis] += cross_offset;
        cursor[main_axis] += child_size[main_axis] + spacing;
        Rect child_frame = {pos, child_size};
        child.display(child_frame);
#ifndef NDEBUG
        if (have_overflow)
            overflow_debug_info_child(&child, child_frame, main_axis);
#endif
    }
}

vml::float2 StackView::do_calc_intrinsic_size(vml::float2 max_size) const {
    vml::float2 const item_spacing = ImGui::GetStyle().ItemSpacing;
    unsigned main_axis = (unsigned)m_main_axis;
    unsigned cross_axis = 1 - main_axis;
    vml::float2 result = {0.0f, 0.0f};
    for (auto& child : m_children) {
        SizeReq2 size_req = child.size_requirement();
        vml::float2 child_max_size = max_size;
        child_max_size[main_axis] = -1.0f;
        vml::float2 child_size = child.calc_intrinsic_size(child_max_size);
        vml::float2 resolved_size =
            vml::map(size_req, child_size, vml::float2(-1.0f, -1.0f),
                     resolve_child_size);
        result[main_axis] += resolved_size[main_axis];
        result[cross_axis] =
            std::max(result[cross_axis], resolved_size[cross_axis]);
    }
    result[main_axis] +=
        size_one_less(m_children.size()) * item_spacing[main_axis];
    return result;
}

Owner<StackView> ui::vstack(ViewContainer<> children,
                            std::optional<Alignment> align_override) {
    return allocate<StackView>(Axis::vertical, std::move(children),
                               align_override);
}

Owner<StackView> ui::hstack(ViewContainer<> children,
                            std::optional<Alignment> align_override) {
    return allocate<StackView>(Axis::horizontal, std::move(children),
                               align_override);
}

ScrollView::ScrollView(Axis main_axis, ViewContainer<>&& children)
    : View(default_size_requirement)
    , m_main_axis(main_axis)
    , m_children(std::move(children)) {}

void ScrollView::do_display(Rect frame) {
    unsigned main_axis = (unsigned)m_main_axis;
    unsigned cross_axis = 1 - main_axis;
    auto& style = ImGui::GetStyle();
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    int window_flags = 0;
    if (m_main_axis == Axis::horizontal)
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    vml::float2 content_size;
    content_size[main_axis] = 0.0f;
    content_size[cross_axis] = frame.size[cross_axis] -
                               2.0f * style.WindowPadding[cross_axis] -
                               style.ScrollbarSize;
    ImGui::SetNextWindowContentSize(content_size);
    bool visible =
        ImGui::BeginChild("##scroll-view", frame.size,
                          ImGuiChildFlags_AlwaysUseWindowPadding, window_flags);
    utl::scope_guard guard = [] {
        ImGui::EndChild();
        ImGui::PopID();
    };
    if (!visible) return;
    vml::float2 size_avail = ImGui::GetContentRegionAvail();
    size_avail[main_axis] = -1.0f;
    float spacing = style.ItemSpacing[main_axis];
    vml::float2 cursor = ImGui::GetCursorScreenPos();
    for (auto& child : m_children) {
        vml::float2 intrinsic_size = child.calc_intrinsic_size(size_avail);
        SizeReq2 size_req = child.size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic_size, size_avail, resolve_child_size);
        child.display({cursor, child_size});
        cursor[main_axis] += child_size[main_axis] + spacing;
    }
}

vml::float2 ScrollView::do_calc_intrinsic_size(vml::float2 max_size) const {
    unsigned main_axis = (unsigned)m_main_axis;
    unsigned cross_axis = 1 - main_axis;
    auto& style = ImGui::GetStyle();
    float cross_axis_padding =
        2.0f * style.WindowPadding[cross_axis] + style.ScrollbarSize;
    float extend = 0.0f;
    vml::float2 child_max_size = max_size;
    child_max_size[main_axis] = -1.0f;
    child_max_size[cross_axis] -= cross_axis_padding;
    for (auto& child : m_children) {
        vml::float2 intrinsic = child.calc_intrinsic_size(child_max_size);
        extend = std::max(extend, intrinsic[cross_axis]);
    }
    vml::float2 size = {0.0f, 0.0f};
    size[cross_axis] = extend + cross_axis_padding;
    return size;
}

Owner<ScrollView> ui::vscroll(ViewContainer<> children) {
    return allocate<ScrollView>(Axis::vertical, std::move(children));
}

Owner<ScrollView> ui::hscroll(ViewContainer<> children) {
    return allocate<ScrollView>(Axis::horizontal, std::move(children));
}

TabView::TabView(Array<Tab>&& tabs)
    : View(default_size_requirement)
    , m_tabs(std::move(tabs)) {}

void TabView::do_display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    int window_flags = 0;
    bool visible = ImGui::BeginChild("##tab-view", frame.size,
                                     ImGuiChildFlags_None, window_flags);
    utl::scope_guard window_guard = [] {
        ImGui::EndChild();
        ImGui::PopID();
    };
    if (!visible) return;
    if (!ImGui::BeginTabBar("##tab-bar")) return;
    utl::scope_guard tab_guard = [] { ImGui::EndTabBar(); };
    vml::float2 size_avail = ImGui::GetContentRegionAvail();
    for (auto& [title, child] : m_tabs) {
        auto title_string = title.get();
        if (!ImGui::BeginTabItem(title_string.value().c_str(), nullptr))
            continue;
        vml::float2 intrinsic_size = child->calc_intrinsic_size(size_avail);
        SizeReq2 size_req = child->size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic_size, size_avail, resolve_child_size);
        vml::float2 pos = ImGui::GetCursorScreenPos();
        child->display({pos, child_size});
        ImGui::EndTabItem();
    }
}

vml::float2 TabView::do_calc_intrinsic_size(vml::float2) const {
    return {0.0f, 0.0f};
}

Owner<TabView> ui::tab_view(Array<Tab> tabs) {
    return allocate<TabView>(std::move(tabs));
}

ColumnView::ColumnView(Array<Column>&& columns)
    : View(default_size_requirement)
    , m_columns(std::move(columns)) {}

static void distribute_delta(
    std::span<ui::Column const> columns, std::span<float> widths,
    std::size_t begin, std::size_t end, float delta, float max_layout_width,
    std::size_t preferred = std::numeric_limits<std::size_t>::max()) {
    if (begin >= end || std::abs(delta) < 0.01f) return;
    auto get_bounds = [&](std::size_t i) {
        float const lo = std::max(0.0f, columns[i].min_width);
        float const hi =
            std::max(lo, std::min(columns[i].max_width, max_layout_width));
        return std::pair{lo, hi};
    };
    // First, let the preferred column absorb as much of the delta
    // as possible. Other columns remain untouched if it can absorb
    // the entire change.
    if (preferred >= begin && preferred < end) {
        auto const [lo, hi] = get_bounds(preferred);
        float const old_width = widths[preferred];
        widths[preferred] = std::clamp(old_width + delta, lo, hi);
        delta -= widths[preferred] - old_width;
        if (std::abs(delta) < 0.01f) return;
    }
    // Redistribute only the remaining delta among the other columns.
    for (std::size_t iteration = 0; iteration < end - begin + 1; ++iteration) {
        if (std::abs(delta) < 0.01f) break;
        std::size_t active = 0;
        for (std::size_t i = begin; i < end; ++i) {
            if (i == preferred) continue;
            auto const [lo, hi] = get_bounds(i);
            float const capacity =
                delta > 0.0f ? hi - widths[i] : widths[i] - lo;
            if (capacity > 0.001f) ++active;
        }
        if (active == 0) break;
        float const share = delta / static_cast<float>(active);
        float applied = 0.0f;
        for (std::size_t i = begin; i < end; ++i) {
            if (i == preferred) continue;
            auto const [lo, hi] = get_bounds(i);
            float const capacity =
                delta > 0.0f ? hi - widths[i] : widths[i] - lo;
            if (capacity <= 0.001f) continue;
            float const amount = delta > 0.0f ? std::min(share, capacity)
                                              : -std::min(-share, capacity);
            widths[i] += amount;
            applied += amount;
        }
        if (std::abs(applied) < 0.001f) break;
        delta -= applied;
    }
}

void ColumnView::do_display(Rect frame) {
    if (m_columns.empty()) return;

    auto& style = ImGui::GetStyle();
    vml::float2 const window_padding = style.WindowPadding;
    float const splitter_width = 2.0f * style.SeparatorSize;

    std::size_t count = m_columns.size();
    std::size_t const splitter_count = count - 1;
    const float usable_width =
        std::max(0.0f, frame.size.x - splitter_width * splitter_count);
    ImGuiID const count_id = ImGui::GetID("##column_count");
    ImGuiID const total_id = ImGui::GetID("##column_total_width");
    utl::small_vector<ImGuiID, 8> const width_ids = [count] {
        utl::small_vector<ImGuiID, 8> ids;
        ids.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            ImGui::PushID((int)i);
            ids.push_back(ImGui::GetID("##column_width"));
            ImGui::PopID();
        }
        return ids;
    }();
    utl::small_vector<float, 8> widths(count);

    auto clamp_width = [&](std::size_t i, float width) {
        float const lo = std::max(0.0f, m_columns[i].min_width);
        float const hi = std::max(lo, m_columns[i].max_width);
        return std::clamp(width, lo, hi);
    };

    ImGui::PushID(this);
    utl::scope_guard id_guard = [] { ImGui::PopID(); };

    ImGuiStorage* storage = ImGui::GetStateStorage();
    // Keep widths in ImGui's per-window state storage.
    // Widths are stored in pixels; resizing the parent redistributes
    // the difference while respecting the column constraints.
    bool const is_init = (std::size_t)storage->GetInt(count_id, -1) == count &&
                         storage->GetFloat(total_id, -1.0f) >= 0.0f;

    if (!is_init) {
        float const initial_width = usable_width / static_cast<float>(count);
        for (std::size_t i = 0; i < count; ++i)
            widths[i] = clamp_width(i, initial_width);
        float sum = 0.0f;
        for (float width : widths)
            sum += width;
        distribute_delta(m_columns, widths, 0, count, usable_width - sum,
                         usable_width);
        storage->SetInt(count_id, count);
    } else {
        for (std::size_t i = 0; i < count; ++i)
            widths[i] = clamp_width(i, storage->GetFloat(width_ids[i], 0.0f));
        float sum = 0.0f;
        for (float width : widths)
            sum += width;
        // Adapt to parent-window resizing.
        distribute_delta(m_columns, widths, 0, count, usable_width - sum,
                         usable_width);
    }

    // ------------------------------------------------------------
    // Phase 1: Process splitter interactions, then render splitters.
    // Hitboxes use the original positions for the entire interaction
    // pass. Visual separators use the final positions.
    // ------------------------------------------------------------
    // Snapshot the widths so interaction hitboxes don't move mid-pass.
    utl::small_vector<float, 8> original_widths(count);
    for (std::size_t i = 0; i < count; ++i)
        original_widths[i] = widths[i];
    utl::small_vector<float, 8> splitter_xs(splitter_count);
    float splitter_x = frame.origin.x;
    for (std::size_t i = 0; i < splitter_count; ++i) {
        splitter_x += original_widths[i];
        splitter_xs[i] = splitter_x;
        splitter_x += splitter_width;
    }
    // Capture colors before making SplitterBehavior's rendering
    // transparent.
    ImU32 const separator_color = ImGui::GetColorU32(ImGuiCol_Separator);
    ImU32 const hovered_color = ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
    ImU32 const active_color = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
    utl::small_vector<ImU32, 8> splitter_colors(splitter_count);
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive, ImVec4(0, 0, 0, 0));
    for (std::size_t i = 0; i < splitter_count; ++i) {
        // Use the original group widths for this interaction pass.
        float left_width = 0.0f;
        float right_width = 0.0f;
        float left_min = 0.0f;
        float left_max = 0.0f;
        float right_min = 0.0f;
        float right_max = 0.0f;
        for (std::size_t j = 0; j <= i; ++j) {
            left_width += original_widths[j];
            float const lo = std::max(0.0f, m_columns[j].min_width);
            float const hi =
                std::max(lo, std::min(m_columns[j].max_width, usable_width));
            left_min += lo;
            left_max += hi;
        }
        for (std::size_t j = i + 1; j < count; ++j) {
            right_width += original_widths[j];
            float const lo = std::max(0.0f, m_columns[j].min_width);
            float const hi =
                std::max(lo, std::min(m_columns[j].max_width, usable_width));
            right_min += lo;
            right_max += hi;
        }
        float const pair_width = left_width + right_width;
        float const effective_left_min =
            std::max(left_min, pair_width - right_max);
        float const effective_right_min =
            std::max(right_min, pair_width - left_max);
        ImRect const bb(ImVec2(splitter_xs[i], frame.origin.y),
                        ImVec2(splitter_xs[i] + splitter_width,
                               frame.origin.y + frame.size.y));
        ImGuiID const splitter_id =
            ImGui::GetID(("##splitter_" + std::to_string(i)).c_str());
        float new_left_width = left_width;
        float new_right_width = right_width;
        ImGui::SplitterBehavior(bb, splitter_id, ImGuiAxis_X, &new_left_width,
                                &new_right_width, effective_left_min,
                                effective_right_min,
                                std::max(2.0f, window_padding.x), 0.0f);
        // Capture interaction state immediately, while this splitter
        // is still the last submitted item.
        bool const hovered = ImGui::IsItemHovered();
        bool const active = ImGui::IsItemActive();
        splitter_colors[i] = active    ? active_color
                             : hovered ? hovered_color
                                       : separator_color;
        // Only the active splitter can change the layout.
        // Redistribute its delta across both groups, respecting limits.
        if (active) {
            float const delta = new_left_width - left_width;
            distribute_delta(m_columns, widths, 0, i + 1, delta, usable_width,
                             i);
            distribute_delta(m_columns, widths, i + 1, count, -delta,
                             usable_width, i + 1);
        }
    }
    ImGui::PopStyleColor(3);

    // Draw all separators at their final positions.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float final_x = frame.origin.x;
    for (std::size_t i = 0; i < splitter_count; ++i) {
        final_x += widths[i];
        vml::float2 min = {final_x, frame.origin.y};
        vml::float2 max = {final_x + splitter_width,
                           frame.origin.y + frame.size.y};
        draw_list->AddRectFilled(min, max, splitter_colors[i]);
        final_x += splitter_width;
    }

    // Persist final widths after handling all splitter interactions.
    for (std::size_t i = 0; i < count; ++i)
        storage->SetFloat(width_ids[i], widths[i]);
    storage->SetFloat(total_id, frame.size.x);

    // ------------------------------------------------------------
    // Phase 2: render bodies.
    // ------------------------------------------------------------
    float x = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        vml::float2 column_pos = {frame.origin.x + x, frame.origin.y};
        vml::float2 column_size = {widths[i], frame.size.y};
        m_columns[i].content->display({column_pos, column_size});
        x += widths[i];
        if (i < splitter_count) x += splitter_width;
    }
}

vml::float2 ColumnView::do_calc_intrinsic_size(vml::float2 max_size) const {
    vml::float2 size = {0.0f, 0.0f};
    vml::float2 child_max_size = {-1.0f, max_size.y};
    for (auto& column : m_columns) {
        vml::float2 column_size =
            column.content->calc_intrinsic_size(child_max_size);
        size.x += std::max(column.min_width, column_size.x);
        size.y = std::max(size.y, column_size.y);
    }
    return size;
}

Owner<ColumnView> ui::columns(Array<Column> columns) {
    return allocate<ColumnView>(std::move(columns));
}

template <typename Index>
SwitchView<Index>::SwitchView(Property<Index> index, ViewContainer<>&& children)
    : View(default_size_requirement)
    , m_index(std::move(index))
    , m_children(std::move(children)) {}

template <typename Index> void SwitchView<Index>::do_display(Rect frame) {
    Index index = m_index.materialize();
    if (index < 0 || (std::size_t)index >= m_children.size()) {
        Logger::error("SwitchView: index out of range");
        return;
    }
    auto& active_child = m_children[(std::size_t)index];
    active_child.display(frame);
}

template <typename Index>
vml::float2
SwitchView<Index>::do_calc_intrinsic_size(vml::float2 max_size) const {
    vml::float2 size = {0.0f, 0.0f};
    for (auto& child : m_children)
        size = vml::max(size, child.calc_intrinsic_size(max_size));
    return size;
}

template class ui::SwitchView<int>;
template class ui::SwitchView<bool>;

namespace {

struct NullView : View {
    NullView()
        : View({SizeReq::fit(), SizeReq::fit()}) {}
    void do_display(Rect) override {}
    vml::float2 do_calc_intrinsic_size(vml::float2) const override {
        return {0.0f, 0.0f};
    }
};

} // namespace

Owner<ConditionalView> ui::if_view(Property<bool> condition,
                                   Owner<View> then_view) {
    return if_view(std::move(condition), std::move(then_view),
                   allocate<NullView>());
}

Owner<ConditionalView> ui::if_view(Property<bool> condition,
                                   Owner<View> then_view,
                                   Owner<View> else_view) {
    return allocate<ConditionalView>(
        std::move(condition),
        ViewContainer<>({std::move(else_view), std::move(then_view)}));
}

Owner<SwitchView<>> ui::switch_view(Property<int> index,
                                    ViewContainer<> cases) {
    return allocate<SwitchView<>>(std::move(index), std::move(cases));
}
