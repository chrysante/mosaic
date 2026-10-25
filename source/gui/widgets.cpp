#include "gui/widgets.h"
#include "gui/view_support.h"

#include <imgui.h>
#include <imgui_internal.h>

using namespace mosaic;
using namespace ui;

Button::Button(std::string&& label, std::function<void()>&& on_click)
    : View({SizeReq::fit(), SizeReq::fit()})
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

Ref<Button> ui::button(std::string label, std::function<void()> on_click) {
    return create<Button>(std::move(label), std::move(on_click));
}

Checkbox::Checkbox(Binding<bool>&& state)
    : View({SizeReq::fit(), SizeReq::fit()})
    , m_state(std::move(state)) {}

void Checkbox::display(Rect frame) {
    ImGui::SetCursorScreenPos(frame.origin);
    bool current_value = m_state.get();
    if (ImGui::Checkbox("##checkbox", &current_value))
        m_state.on_change(current_value);
}

vml::float2 Checkbox::intrinsic_size() const {
    auto& style = ImGui::GetStyle();
    return vml::float2(ImGui::GetFontSize()) +
           2.0f * (vml::float2)style.FramePadding;
}

Ref<Checkbox> ui::checkbox(Binding<bool> state) {
    return create<Checkbox>(std::move(state));
}

static SizeReq2 separator_size_req(Orientation orientation) {
    switch (orientation) {
    case Orientation::horizontal:
        return {SizeReq::fill(), SizeReq::fit()};
    case Orientation::vertical:
        return {SizeReq::fit(), SizeReq::fill()};
    }
}

Separator::Separator(Orientation orientation)
    : View(separator_size_req(orientation))
    , m_orientation(orientation) {}

void Separator::display(Rect frame) {
    auto& style = ImGui::GetStyle();
    auto* DL = ImGui::GetWindowDrawList();
    switch (m_orientation) {
    case Orientation::horizontal:
        DL->AddLineH(
            frame.min().x, frame.max().x, frame.origin.y + 0.5f * frame.size.y,
            ImGui::GetColorU32(ImGuiCol_Separator), style.SeparatorSize);
        break;
    case Orientation::vertical:
        DL->AddLineV(frame.origin.x + 0.5f * frame.size.x, frame.min().y,
                     frame.max().y, ImGui::GetColorU32(ImGuiCol_Separator),
                     style.SeparatorSize);
        break;
    }
}

vml::float2 Separator::intrinsic_size() const {
    auto& style = ImGui::GetStyle();
    switch (m_orientation) {
    case Orientation::horizontal:
        return {0.0f, style.SeparatorSize + 2.0f * style.ItemSpacing.y};
    case Orientation::vertical:
        return {style.SeparatorSize + 2.0f * style.ItemSpacing.x, 0.0f};
    }
}

Ref<Separator> ui::hseparator() {
    return create<Separator>(Orientation::horizontal);
}

Ref<Separator> ui::vseparator() {
    return create<Separator>(Orientation::vertical);
}

Spacer::Spacer(float weight)
    : View({SizeReq::fill(weight), SizeReq::fill(weight)}) {}

void Spacer::display(Rect) {}

vml::float2 Spacer::intrinsic_size() const { return {0.0f, 0.0f}; }

Ref<Spacer> ui::spacer(float weight) { return create<Spacer>(weight); }
