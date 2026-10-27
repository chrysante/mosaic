#pragma once

#include <cfloat>
#include <functional>

#include <vml/vml.hpp>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// Text
// =============================================================================

struct TextOptions {
    /// Wrapping
    bool wrap = false;
    /// In many edge cases the layout algorithm computes a suboptimal frame for
    /// the text. Enable this option to make the text scrollable in this case.
    /// This only applies when `wrap` is enabled.
    bool scrollable_on_clip = true;
    float default_wrap_width = 300.0f;
    float max_wrap_width = FLT_MAX;
};

class TextView : public View {
public:
    explicit TextView(Property<std::string const>&& text,
                      TextOptions options = {});

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    TextOptions m_options;
    Property<std::string const> m_text;
};

Owner<TextView> text(Property<std::string const> text_arg,
                     TextOptions options = {});

// =============================================================================
// Button
// =============================================================================

class ButtonView : public View {
public:
    explicit ButtonView(Property<std::string const>&& label,
                        std::function<void()>&& on_click);

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<std::string const> m_label;
    std::function<void()> m_on_click;
};

Owner<ButtonView> button(Property<std::string const> label,
                         std::function<void()> action = {});

// =============================================================================
// Checkbox
// =============================================================================

class CheckboxView : public View {
public:
    explicit CheckboxView(Property<bool>&& state);

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<bool> m_state;
};

Owner<CheckboxView> checkbox(Property<bool> state);

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
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<T> m_state;
    Property<ScalarType const> m_min;
    Property<ScalarType const> m_max;
    SliderOptions m_options;
};

template <typename T>
Owner<SliderView<T>>
slider(Property<T> state,
       Property<typename SliderView<T>::ScalarType const> min,
       Property<typename SliderView<T>::ScalarType const> max,
       SliderOptions options = {}) {
    return allocate<SliderView<T>>(std::move(state), std::move(min),
                                   std::move(max), std::move(options));
}

// =============================================================================
// ColorEdit
// =============================================================================

struct ColorEditOptions {
    bool alpha = true;
    bool picker = true;
    bool options = true;
    bool small_preview = true;
    bool inputs = true;
    bool tooltip = true;
    bool enable_drag_drop = true;
};

template <std::size_t NumComponents> class ColorEditView : public View {
public:
    using ColorType = vml::vector<float, NumComponents>;

    explicit ColorEditView(Property<ColorType>&& state,
                           ColorEditOptions options = {});

    Property<ColorType> const& get_state() const { return m_state; }

    void set_state(Property<ColorType> state) { m_state = std::move(state); }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<ColorType> m_state;
    ColorEditOptions m_options;
};

Owner<ColorEditView<3>> color_edit(Property<vml::float3> state,
                                   ColorEditOptions options = {});

Owner<ColorEditView<4>> color_edit(Property<vml::float4> state,
                                   ColorEditOptions options = {});

// =============================================================================
// Separator
// =============================================================================

class SeparatorView : public View {
public:
    explicit SeparatorView(Orientation orientation);

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Orientation m_orientation;
};

Owner<SeparatorView> hseparator();

Owner<SeparatorView> vseparator();

// =============================================================================
// Spacer
// =============================================================================

class SpacerView : public View {
public:
    SpacerView(float weight = 1.0f);

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;
};

Owner<SpacerView> spacer(float weight = 1.0f);

// =============================================================================
// Image
// =============================================================================

class ImageView : public View {
public:
    ImageView(void const* image, float aspect_ratio);

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    void const* m_image;
    float m_aspect_ratio;
};

/// \p aspect_ratio means `width / height`
Owner<ImageView> image(void const* image, float aspect_ratio);

} // namespace mosaic::ui