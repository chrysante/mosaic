#pragma once

#include <functional>

#include <vml/vml.hpp>

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
    using DataType = vml::get_underlying_type<T>::type;

    explicit SliderView(Binding<T>&& state, ConstBinding<DataType>&& min,
                        ConstBinding<DataType>&& max,
                        SliderOptions options = {});

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

    Binding<T> const& get_state() const { return m_state; }

    void set_state(Binding<T> state) { m_state = std::move(state); }

private:
    Binding<T> m_state;
    ConstBinding<DataType> m_min;
    ConstBinding<DataType> m_max;
    SliderOptions m_options;
};

template <typename T>
Ref<SliderView<T>> slider(Binding<T> state,
                          ConstBinding<typename SliderView<T>::DataType> min,
                          ConstBinding<typename SliderView<T>::DataType> max,
                          SliderOptions options = {}) {
    return create<SliderView<T>>(std::move(state), std::move(min),
                                 std::move(max), std::move(options));
}

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