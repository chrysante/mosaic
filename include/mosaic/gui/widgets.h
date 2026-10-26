#pragma once

#include <functional>

#include <vml/vml.hpp>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// Text
// =============================================================================

struct TextOptions { bool wrap = false; };

class TextView : public View {
public:
    explicit TextView(Property<std::string const>&& text,
                      TextOptions options = {});

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    TextOptions m_options;
    Property<std::string const> m_text;
};

Ref<TextView> text(Property<std::string const> text_arg,
                   TextOptions options = {});

// =============================================================================
// Button
// =============================================================================

class ButtonView : public View {
public:
    explicit ButtonView(std::string&& label, std::function<void()>&& on_click);

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    std::string m_label;
    std::function<void()> m_on_click;
};

Ref<ButtonView> button(std::string label, std::function<void()> action = {});

// =============================================================================
// Checkbox
// =============================================================================

class CheckboxView : public View {
public:
    explicit CheckboxView(Property<bool>&& state);

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    Property<bool> m_state;
};

Ref<CheckboxView> checkbox(Property<bool> state);

// =============================================================================
// Slider
// =============================================================================

struct SliderOptions {
    // Only used for floating point sliders
    std::string format = "%0.1f";

    bool logarithmic = false;
    bool round_to_format = true;
    bool input = true;
    bool wrap_around = false;
    bool clamp_on_input = false;
    bool clamp_zero_range = false;
    bool speed_tweaks = true;
    bool color_markers = true;
};

template <typename T> class SliderView : public View {
public:
    using ScalarType = vml::get_underlying_type<T>::type;

    explicit SliderView(Property<T>&& state, Property<ScalarType const>&& min,
                        Property<ScalarType const>&& max,
                        SliderOptions options = {});

    Property<T> const& get_state() const { return m_state; }

    void set_state(Property<T> state) { m_state = std::move(state); }

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    Property<T> m_state;
    Property<ScalarType const> m_min;
    Property<ScalarType const> m_max;
    SliderOptions m_options;
};

template <typename T>
Ref<SliderView<T>>
slider(Property<T> state,
       Property<typename SliderView<T>::ScalarType const> min,
       Property<typename SliderView<T>::ScalarType const> max,
       SliderOptions options = {}) {
    return create<SliderView<T>>(std::move(state), std::move(min),
                                 std::move(max), std::move(options));
}

// =============================================================================
// Separator
// =============================================================================

class SeparatorView : public View {
public:
    explicit SeparatorView(Orientation orientation);

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    Orientation m_orientation;
};

Ref<SeparatorView> hseparator();

Ref<SeparatorView> vseparator();

// =============================================================================
// Spacer
// =============================================================================

class SpacerView : public View {
public:
    SpacerView(float weight = 1.0f);

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;
};

Ref<SpacerView> spacer(float weight = 1.0f);

} // namespace mosaic::ui