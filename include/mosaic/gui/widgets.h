#pragma once

#include <functional>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// Spacer
// =============================================================================

class Spacer : public View {
public:
    Spacer();
    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;
};

Ref<Spacer> spacer();

// =============================================================================
// Button
// =============================================================================

class Button : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit Button(std::string&& label, std::function<void()>&& on_click,
                    SizeReq2 size_requirement = default_layout_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::string m_label;
    std::function<void()> m_on_click;
};

Ref<Button>
button(std::string label, std::function<void()> action = {},
       SizeReq2 size_requirement = Button::default_layout_requirement);

// =============================================================================
// Checkbox
// =============================================================================

class Checkbox : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit Checkbox(std::function<bool()>&& state,
                      std::function<void(bool)>&& on_change,
                      SizeReq2 size_requirement = default_layout_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::function<bool()> m_state;
    std::function<void(bool)> m_on_change;
};

Ref<Checkbox>
checkbox(std::function<bool()> state, std::function<void(bool)> on_change,
         SizeReq2 size_requirement = Checkbox::default_layout_requirement);

Ref<Checkbox>
checkbox(bool* value,
         SizeReq2 size_requirement = Checkbox::default_layout_requirement);

} // namespace mosaic::ui