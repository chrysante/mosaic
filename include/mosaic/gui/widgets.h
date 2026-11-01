#pragma once

#include <cfloat>
#include <functional>
#include <string>
#include <vector>

#include <vml/vml.hpp>

#include <mosaic/gui/icon.h>
#include <mosaic/gui/style.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>
#include <mosaic/support/memory.h>

namespace mosaic::ui {

struct TextOptions {
    /// Wrapping
    bool wrap = false;

    /// In many edge cases the layout algorithm computes a suboptimal frame for
    /// the text. Enable this option to make the text scrollable in this case.
    /// This only applies when `wrap` is enabled.
    bool scrollable_on_clip = true;

    /// Add frame padding around the text
    bool is_label = false;

    float default_wrap_width = 300.0f;
    float max_wrap_width = FLT_MAX;
};

MOSAIC_EXPORT Owner<View> text(Property<std::string const> text_arg,
                               TextOptions options = {});

MOSAIC_EXPORT Owner<View> label(Property<std::string const> text_arg);

struct TextInputOptions { int num_lines = 1; };

MOSAIC_EXPORT Owner<View> text_input(std::string* buffer,
                                     TextInputOptions options = {});

MOSAIC_EXPORT Owner<View> icon(Property<Icon const> icon);

struct ButtonOptions { ControlColor const* color = nullptr; };

MOSAIC_EXPORT Owner<View> button(Property<std::string const> label,
                                 std::function<void()> action = {},
                                 ButtonOptions options = {});

MOSAIC_EXPORT Owner<View> button(Property<Icon const> icon,
                                 std::function<void()> action = {});

MOSAIC_EXPORT Owner<View> checkbox(Property<bool> state);

enum class ComboHeight { small, regular, large, giant };

struct ComboOptions {
    ComboHeight height = ComboHeight::regular;
    bool arrow_button = true;
};

MOSAIC_EXPORT Owner<View> combo(Property<int> selection,
                                Property<std::string const> preview,
                                Property<std::vector<std::string> const> items,
                                ComboOptions options = {});

struct SliderOptions {
    /// printf-style format string
    char const* format = nullptr;

    bool logarithmic = false;
    bool round_to_format = true;
    bool input = true;
    bool wrap_around = false;
    bool clamp_on_input = false;
    bool clamp_zero_range = false;
    bool speed_tweaks = true;
    bool color_markers = false;
};

template <typename T> using ScalarType = vml::get_underlying_type<T>::type;

template <typename T>
Owner<View> slider(Property<T> state, Property<ScalarType<T> const> min,
                   Property<ScalarType<T> const> max,
                   SliderOptions options = {});

struct ColorEditOptions {
    bool alpha = true;
    bool picker = true;
    bool options = true;
    bool small_preview = true;
    bool inputs = true;
    bool tooltip = true;
    bool enable_drag_drop = true;
};

MOSAIC_EXPORT Owner<View> color_edit(Property<vml::float3> state,
                                     ColorEditOptions options = {});

MOSAIC_EXPORT Owner<View> color_edit(Property<vml::float4> state,
                                     ColorEditOptions options = {});

MOSAIC_EXPORT Owner<View> hseparator();

MOSAIC_EXPORT Owner<View> vseparator();

MOSAIC_EXPORT Owner<View> spacer(float weight = 1.0f);

/// \p aspect_ratio means `width / height`
MOSAIC_EXPORT Owner<View> image(void const* image, float aspect_ratio);

} // namespace mosaic::ui