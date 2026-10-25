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

void StackView::display(Rect frame) {
    if (m_children.empty()) return;
    std::size_t main_axis = (std::size_t)m_orientation;
    std::size_t other_axis = 1 - main_axis;
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
        if (child_size.map([](float v) { return v > 0; }).all())
            child.display({pos, child_size});
        cursor[main_axis] += child_size[main_axis] + spacing;
    }
}

vml::float2 StackView::intrinsic_size() const {
    vml::float2 const item_spacing = ImGui::GetStyle().ItemSpacing;
    std::size_t main_axis = (std::size_t)m_orientation;
    std::size_t other_axis = 1 - main_axis;
    vml::float2 result = {0.0f, 0.0f};
    for (auto& child : m_children) {
        vml::float2 child_size = child.intrinsic_size();
        result[main_axis] += child_size[main_axis];
        result[other_axis] =
            std::max(result[other_axis], child_size[other_axis]);
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

void ScrollView::display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    int window_flags = 0;
    if (m_orientation == Orientation::horizontal)
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    bool visible = ImGui::BeginChild("##scroll-view", frame.size,
                                     ImGuiChildFlags_None, window_flags);
    utl::scope_guard guard = [] {
        ImGui::EndChild();
        ImGui::PopID();
    };
    if (!visible) return;
    std::size_t main_axis = (std::size_t)m_orientation;
    vml::float2 size_avail = ImGui::GetContentRegionAvail();
    size_avail[main_axis] = -1.0f;
    float spacing = ImGui::GetStyle().ItemSpacing[main_axis];
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

vml::float2 ScrollView::intrinsic_size() const { return {500, 500}; }

Ref<ScrollView> ui::vscroll(ViewContainer<> children) {
    return create<ScrollView>(Orientation::vertical, std::move(children));
}

Ref<ScrollView> ui::hscroll(ViewContainer<> children) {
    return create<ScrollView>(Orientation::horizontal, std::move(children));
}
