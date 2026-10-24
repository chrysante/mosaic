#include "gui/view.h"
#include "utl/common.hpp"
#include "vml/vector.hpp"

#include <cfloat>

#include <imgui.h>
#include <imgui_internal.h>
#include <utility>
#include <utl/scope_guard.hpp>

using namespace mosaic;
using namespace ui;

Button::Button(std::string&& label, std::function<void()>&& on_click,
               SizeReq2 size_requirement)
    : View(size_requirement)
    , m_label(std::move(label))
    , m_on_click(std::move(on_click)) {}

void Button::display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    if (ImGui::ButtonEx(m_label.c_str(), frame.size) && m_on_click)
        m_on_click();
}

vml::float2 Button::intrinsic_size() const {
    auto& style = ImGui::GetStyle();
    vml::float2 text_size = ImGui::CalcTextSize(
        m_label.data(), m_label.data() + m_label.size(), true);
    return text_size + 2.0f * (vml::float2)style.FramePadding;
}

Ref<Button> ui::button(std::string label, std::function<void()> on_click,
                       SizeReq2 size_requirement) {
    return create<Button>(std::move(label), std::move(on_click),
                          size_requirement);
}

Checkbox::Checkbox(std::function<bool()>&& state,
                   std::function<void(bool)>&& on_change,
                   SizeReq2 size_requirement)
    : View(size_requirement)
    , m_state(std::move(state))
    , m_on_change(std::move(on_change)) {}

void Checkbox::display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    bool current_value = m_state ? m_state() : false;
    if (ImGui::Checkbox("##checkbox", &current_value) && m_on_change)
        m_on_change(current_value);
}

vml::float2 Checkbox::intrinsic_size() const {
    auto& style = ImGui::GetStyle();
    return vml::float2(ImGui::GetFontSize()) +
           2.0f * (vml::float2)style.FramePadding;
}

Ref<Checkbox> ui::checkbox(std::function<bool()> state,
                           std::function<void(bool)> on_change,
                           SizeReq2 size_requirement) {
    return create<Checkbox>(std::move(state), std::move(on_change),
                            size_requirement);
}

Ref<Checkbox> ui::checkbox(bool* value, SizeReq2 size_requirement) {
    return ui::checkbox([value] { return *value; },
                        [value](bool new_value) { *value = new_value; },
                        size_requirement);
}

Spacer::Spacer()
    : View({SizeReq::fill(), SizeReq::fill()}) {}

void Spacer::display(Rect) {}

vml::float2 Spacer::intrinsic_size() const { return {0.0f, 0.0f}; }

Ref<Spacer> ui::spacer() { return create<Spacer>(); }

static float resolve_child_size(SizeReq requirement, float intrinsic,
                                float available) {
    switch (requirement.type()) {
    case SizeReq::Type::fixed:
        return requirement.value();
    case SizeReq::Type::fit:
        return intrinsic;
    case SizeReq::Type::fill:
        return available >= 0 ? available : intrinsic;
    }
}

StackView::StackView(Orientation orientation, ViewContainer&& children,
                     SizeReq2 size_requirement)
    : View(size_requirement)
    , m_orientation(orientation)
    , m_children(std::move(children)) {}

void StackView::display(Rect frame) {
    if (m_children.empty()) return;
    std::size_t main_axis = (std::size_t)m_orientation;
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
        child.display({cursor, child_size});
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

Ref<StackView> ui::vstack(ViewContainer children, SizeReq2 size_requirement) {
    return create<StackView>(StackView::Orientation::vertical,
                             std::move(children), size_requirement);
}

Ref<StackView> ui::hstack(ViewContainer children, SizeReq2 size_requirement) {
    return create<StackView>(StackView::Orientation::horizontal,
                             std::move(children), size_requirement);
}

VScrollView::VScrollView(ViewContainer&& children, SizeReq2 size_requirement)
    : View(size_requirement)
    , m_children(std::move(children)) {}

void VScrollView::display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    bool visible =
        ImGui::BeginChild("##scroll-view", frame.size, ImGuiChildFlags_None,
                          ImGuiWindowFlags_None);
    utl::scope_guard guard = [] {
        ImGui::EndChild();
        ImGui::PopID();
    };
    if (!visible) return;
    vml::float2 size_avail = {ImGui::GetContentRegionAvail().x, -1.0f};
    float spacing = ImGui::GetStyle().ItemSpacing.y;
    vml::float2 cursor = ImGui::GetCursorScreenPos();
    for (auto& child : m_children) {
        vml::float2 intrinsic_size = child.intrinsic_size();
        SizeReq2 size_req = child.size_requirement();
        vml::float2 child_size =
            vml::map(size_req, intrinsic_size, size_avail, resolve_child_size);
        child.display({cursor, child_size});
        cursor.y += child_size.y + spacing;
    }
}

vml::float2 VScrollView::intrinsic_size() const { return {500, 500}; }

Ref<VScrollView> ui::vscroll(ViewContainer children,
                             SizeReq2 size_requirement) {
    return create<VScrollView>(std::move(children), size_requirement);
}
