#pragma once

#include <functional>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// Button
// =============================================================================

class Button : public View {
public:
    explicit Button(std::string&& label, std::function<void()>&& on_click);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::string m_label;
    std::function<void()> m_on_click;
};

Ref<Button> button(std::string label, std::function<void()> action = {});

// =============================================================================
// Checkbox
// =============================================================================

class Checkbox : public View {
public:
    explicit Checkbox(std::function<bool()>&& state,
                      std::function<void(bool)>&& on_change);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::function<bool()> m_state;
    std::function<void(bool)> m_on_change;
};

Ref<Checkbox> checkbox(std::function<bool()> state,
                       std::function<void(bool)> on_change);

Ref<Checkbox> checkbox(bool* value);

// =============================================================================
// Spacer
// =============================================================================

class Spacer : public View {
public:
    Spacer(float weight = 1.0f);
    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;
};

Ref<Spacer> spacer(float weight = 1.0f);

} // namespace mosaic::ui