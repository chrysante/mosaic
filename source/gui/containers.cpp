#include "gui/containers.h"

#include <algorithm>
#include <mach/memory_object_types.h>
#include <span>

#include <imgui.h>
#include <utl/scope_guard.hpp>

#include "gui/view_support.h"
#include "utl/common.hpp"

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

StackView::StackView(Orientation orientation, ViewContainer<>&& children,
                     std::optional<Alignment> align_override)
    : View(default_size_requirement)
    , m_orientation(orientation)
    , m_align_override(align_override)
    , m_children(std::move(children)) {}

void handle_overflow(std::span<vml::float2> intrinsic_sizes,
                     std::size_t main_axis, float overflow) {
    utl::small_vector<unsigned, 32> indices(intrinsic_sizes.size(),
                                            utl::no_init);
    for (unsigned i = 0; i < intrinsic_sizes.size(); ++i)
        indices[i] = i;
    std::sort(indices.begin(), indices.end(),
              [intrinsic_sizes, main_axis](unsigned i, unsigned j) {
                  return intrinsic_sizes[i][main_axis] <
                         intrinsic_sizes[j][main_axis];
              });
    std::size_t num_remaining = intrinsic_sizes.size();
    float share = overflow / (float)num_remaining;
    for (unsigned i : indices) {
        --num_remaining;
        float& value = intrinsic_sizes[i][main_axis];
        assert(value >= 0);
        if (value >= share) {
            value -= share;
            overflow -= share;
            continue;
        }
        overflow -= value;
        value = 0.0f;
        share = overflow / (float)num_remaining;
    }
}

void StackView::do_display(Rect frame) {
    if (m_children.empty()) return;
    unsigned main_axis = (unsigned)m_orientation;
    unsigned other_axis = 1 - main_axis;
    float spacing = ImGui::GetStyle().ItemSpacing[main_axis];
    vml::float2 size_avail = frame.size;
    // Measure children and account for spacing.
    utl::small_vector<vml::float2, 32> intrinsic_sizes(m_children.size(),
                                                       utl::no_init);
    float fixed_main = 0.0f;
    float fill_weight_sum = 0;
    for (size_t i = 0, end = m_children.size(); i < end; ++i) {
        auto& child = m_children[i];
        auto intrinsic_size = intrinsic_sizes[i] = child.intrinsic_size();
        auto requirement = child.size_requirement();
        if (requirement[main_axis].is_fill())
            fill_weight_sum += requirement[main_axis].fill_weight();
        fixed_main += resolve_child_size(requirement[main_axis],
                                         intrinsic_size[main_axis], -1.0f);
    }
    float remaining =
        size_avail[main_axis] - fixed_main - spacing * (m_children.size() - 1);
    bool have_overflow = remaining < 0;
    if (have_overflow) handle_overflow(intrinsic_sizes, main_axis, -remaining);
    vml::float2 cursor = frame.origin;
    for (size_t i = 0, end = m_children.size(); i < end; ++i) {
        auto& child = m_children[i];
        auto intrinsic = intrinsic_sizes[i];
        auto size_req = child.size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic, size_avail, resolve_child_size);
        // Filling children share the remaining main-axis space.
        if (size_req[main_axis].is_fill()) {
            child_size[main_axis] = intrinsic[main_axis];
            if (!have_overflow) {
                float fill_share =
                    size_req[main_axis].fill_weight() / fill_weight_sum;
                child_size[main_axis] += std::max(0.0f, remaining * fill_share);
            }
        }
        float offset = [&] {
            auto alignment = m_align_override.value_or(
                child.alignment_requirement()[other_axis]);
            switch (alignment) {
            case Alignment::left:
                return 0.0f;
            case Alignment::center:
                return 0.5f * (frame.size[other_axis] - child_size[other_axis]);
            case Alignment::right:
                return frame.size[other_axis] - child_size[other_axis];
            }
        }();
        vml::float2 pos = cursor;
        pos[other_axis] += offset;
        child.display({pos, child_size});
        cursor[main_axis] += child_size[main_axis] + spacing;
    }
}

vml::float2 StackView::calc_intrinsic_size() const {
    vml::float2 const item_spacing = ImGui::GetStyle().ItemSpacing;
    unsigned main_axis = (unsigned)m_orientation;
    unsigned other_axis = 1 - main_axis;
    vml::float2 result = {0.0f, 0.0f};
    for (auto& child : m_children) {
        SizeReq2 size_req = child.size_requirement();
        vml::float2 child_size = child.intrinsic_size();
        vml::float2 resolved_size =
            vml::map(size_req, child_size, vml::float2(-1.0f, -1.0f),
                     resolve_child_size);
        result[main_axis] += resolved_size[main_axis];
        result[other_axis] =
            std::max(result[other_axis], resolved_size[other_axis]);
    }
    result[main_axis] +=
        (float)(m_children.size() - 1) * item_spacing[main_axis];
    return result;
}

Ref<StackView> ui::vstack(ViewContainer<> children,
                          std::optional<Alignment> align_override) {
    return create<StackView>(Orientation::vertical, std::move(children),
                             align_override);
}

Ref<StackView> ui::hstack(ViewContainer<> children,
                          std::optional<Alignment> align_override) {
    return create<StackView>(Orientation::horizontal, std::move(children),
                             align_override);
}

ScrollView::ScrollView(Orientation orientation, ViewContainer<>&& children)
    : View(default_size_requirement)
    , m_orientation(orientation)
    , m_children(std::move(children)) {}

void ScrollView::do_display(Rect frame) {
    unsigned main_axis = (unsigned)m_orientation;
    unsigned other_axis = 1 - main_axis;
    auto& style = ImGui::GetStyle();
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    int window_flags = 0;
    if (m_orientation == Orientation::horizontal)
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    vml::float2 content_size;
    content_size[main_axis] = 0.0f;
    content_size[other_axis] = frame.size[other_axis] -
                               2.0f * style.WindowPadding[other_axis] -
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
        vml::float2 intrinsic_size = child.intrinsic_size();
        SizeReq2 size_req = child.size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic_size, size_avail, resolve_child_size);
        child.display({cursor, child_size});
        cursor[main_axis] += child_size[main_axis] + spacing;
    }
}

vml::float2 ScrollView::calc_intrinsic_size() const {
    unsigned other_axis = 1 - (unsigned)m_orientation;
    float extend = 0.0f;
    for (auto& child : m_children)
        extend = std::max(extend, child.intrinsic_size()[other_axis]);
    auto& style = ImGui::GetStyle();
    vml::float2 size = {0.0f, 0.0f};
    size[other_axis] =
        extend + 2.0f * style.WindowPadding[other_axis] + style.ScrollbarSize;
    return size;
}

Ref<ScrollView> ui::vscroll(ViewContainer<> children) {
    return create<ScrollView>(Orientation::vertical, std::move(children));
}

Ref<ScrollView> ui::hscroll(ViewContainer<> children) {
    return create<ScrollView>(Orientation::horizontal, std::move(children));
}

TabView::TabView(std::vector<Tab>&& tabs)
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
        if (!ImGui::BeginTabItem(title_string.get().c_str(), nullptr)) continue;
        vml::float2 intrinsic_size = child->intrinsic_size();
        SizeReq2 size_req = child->size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic_size, size_avail, resolve_child_size);
        vml::float2 pos = ImGui::GetCursorScreenPos();
        child->display({pos, child_size});
        ImGui::EndTabItem();
    }
}

vml::float2 TabView::calc_intrinsic_size() const { return {0.0f, 0.0f}; }

Ref<TabView> ui::tab_view(std::vector<Tab> tabs) {
    return create<TabView>(std::move(tabs));
}

#if 0
ColumnView::ColumnView(ViewContainer<>&& children)
    : View(default_size_requirement)
    , m_children(std::move(children)) {}

static void distribute_delta(
    std::span<ui::ColumnLayout const> columns, std::span<float> widths,
    std::size_t begin, std::size_t end, float delta, float max_layout_width,
    std::size_t preferred = std::numeric_limits<std::size_t>::max()) {
    if (begin >= end || std::abs(delta) < 0.01f) return;
    auto get_bounds = [&](std::size_t i) {
        float const lo = ImMax(0.0f, columns[i].min_width);
        float const hi =
            ImMax(lo, ImMin(columns[i].max_width, max_layout_width));
        return std::pair{lo, hi};
    };
    // First, let the preferred column absorb as much of the delta
    // as possible. Other columns remain untouched if it can absorb
    // the entire change.
    if (preferred >= begin && preferred < end) {
        auto const [lo, hi] = get_bounds(preferred);
        float const old_width = widths[preferred];
        widths[preferred] = ImClamp(old_width + delta, lo, hi);
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
            float const amount = delta > 0.0f ? ImMin(share, capacity)
                                              : -ImMin(-share, capacity);
            widths[i] += amount;
            applied += amount;
        }
        if (std::abs(applied) < 0.001f) break;
        delta -= applied;
    }
}

void ColumnView::do_display(Rect frame) {}

vml::float2 ColumnView::calc_intrinsic_size() const {}

Ref<ColumnView> ui::columns(ViewContainer<> children) {
    return create<ColumnView>(std::move(children));
}
#endif