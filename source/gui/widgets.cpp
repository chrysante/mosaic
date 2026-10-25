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

Checkbox::Checkbox(std::function<bool()>&& state,
                   std::function<void(bool)>&& on_change)
    : View({SizeReq::fit(), SizeReq::fit()})
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
                           std::function<void(bool)> on_change) {
    return create<Checkbox>(std::move(state), std::move(on_change));
}

Ref<Checkbox> ui::checkbox(bool* value) {
    return ui::checkbox([value] { return *value; },
                        [value](bool new_value) { *value = new_value; });
}

Spacer::Spacer(float weight)
    : View({SizeReq::fill(weight), SizeReq::fill(weight)}) {}

void Spacer::display(Rect) {}

vml::float2 Spacer::intrinsic_size() const { return {0.0f, 0.0f}; }

Ref<Spacer> ui::spacer(float weight) { return create<Spacer>(weight); }
