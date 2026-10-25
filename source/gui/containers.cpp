#include "gui/containers.h"

#include <span>

#include <imgui.h>
#include <utl/scope_guard.hpp>

#include "gui/view_support.h"

using namespace mosaic;
using namespace ui;

static float resolve_child_size(SizeReq requirement, float intrinsic,
                                float available) {
    switch (requirement.type()) {
    case SizeReq::Type::fixed:
        return requirement.fixed_size();
    case SizeReq::Type::fit:
        return intrinsic;
    case SizeReq::Type::fill:
        return available >= 0 ? available : intrinsic;
    }
}

StackView::StackView(Orientation orientation, ViewContainer<>&& children,
                     Alignment alignment, SizeReq2 size_requirement)
    : View(size_requirement)
    , m_orientation(orientation)
    , m_alignment(alignment)
    , m_children(std::move(children)) {}

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
    std::size_t fill_count = 0;
    for (size_t i = 0, end = m_children.size(); i < end; ++i) {
        auto& child = m_children[i];
        auto intrinsic_size = intrinsic_sizes[i] = child.intrinsic_size();
        auto requirement = child.size_requirement();
        if (requirement[main_axis].is_fill()) ++fill_count;
        else
            fixed_main += resolve_child_size(requirement[main_axis],
                                             intrinsic_size[main_axis], -1.0f);
    }
    float remaining =
        size_avail[main_axis] - fixed_main - spacing * (m_children.size() - 1);
    float fill_size =
        fill_count > 0 ? std::max(0.0f, remaining / (float)fill_count) : 0.0f;
    vml::float2 cursor = frame.origin;
    for (size_t i = 0, end = m_children.size(); i < end; ++i) {
        auto& child = m_children[i];
        auto intrinsic = intrinsic_sizes[i];
        auto size_req = child.size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic, size_avail, resolve_child_size);
        // Fill children share the remaining main-axis space.
        if (size_req[main_axis].type() == SizeReq::Type::fill)
            child_size[main_axis] = fill_size;
        float offset = [&] {
            switch (m_alignment) {
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

Ref<StackView> ui::vstack(ViewContainer<> children, Alignment alignment,
                          SizeReq2 size_requirement) {
    return create<StackView>(Orientation::vertical, std::move(children),
                             alignment, size_requirement);
}

Ref<StackView> ui::hstack(ViewContainer<> children, Alignment alignment,
                          SizeReq2 size_requirement) {
    return create<StackView>(Orientation::horizontal, std::move(children),
                             alignment, size_requirement);
}

ScrollView::ScrollView(Orientation orientation, ViewContainer<>&& children,
                       SizeReq2 size_requirement)
    : View(size_requirement)
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

Ref<ScrollView> ui::vscroll(ViewContainer<> children,
                            SizeReq2 size_requirement) {
    return create<ScrollView>(Orientation::vertical, std::move(children),
                              size_requirement);
}

Ref<ScrollView> ui::hscroll(ViewContainer<> children,
                            SizeReq2 size_requirement) {
    return create<ScrollView>(Orientation::horizontal, std::move(children),
                              size_requirement);
}
