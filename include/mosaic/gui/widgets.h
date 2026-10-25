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
    explicit Checkbox(Binding<bool>&& state);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    Binding<bool> m_state;
};

Ref<Checkbox> checkbox(Binding<bool> state);

// =============================================================================
// Separator
// =============================================================================

class Separator : public View {
public:
    explicit Separator(Orientation orientation);
    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;

private:
    Orientation m_orientation;
};

Ref<Separator> hseparator();

Ref<Separator> vseparator();

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