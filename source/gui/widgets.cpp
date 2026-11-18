#include "gui/widgets.h"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <variant>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <utl/scope_guard.hpp>
#include <variant>

#include "gui/icon.h"
#include "gui/view_support.h"
#include "style/style.h"

using namespace mosaic;
using namespace ui;

static float default_control_width() { return 300.0f; }

static bool positive(vml::float2 size) {
    return size.map([](auto v) { return v > 0.0f; }).all();
}

static bool non_negative(vml::float2 size) {
    return size.map([](auto v) { return v >= 0.0f; }).all();
}

static void push_control_colors() {
    auto color = get_global_style().get_color(style_color::control);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, color.background.value);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                          color.background_hovered.value);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                          color.background_active.value);
    ImGui::PushStyleColor(ImGuiCol_Button, color.background.value);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          color.background_hovered.value);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color.background_active.value);
    ImGui::PushStyleColor(ImGuiCol_Border, color.border.value);
    ImGui::PushStyleColor(ImGuiCol_Text, color.text.value);
}

static void pop_control_colors() { ImGui::PopStyleColor(8); }

template <typename Callback>
static decltype(auto) with_widget_size(vml::float2 size, Callback&& callback) {
    auto& g = *GImGui;
    // Hacky way to adjust the height of the widget
    ImGuiStyle const& style = ImGui::GetStyle();
    float const font_size = ImGui::GetFontSize();
    float const original_padding_y = style.FramePadding.y;
    float const target_height = size.y;
    float adjusted_font_size = font_size;
    float adjusted_padding_y = original_padding_y;
    if (target_height >= font_size + 2.0f * original_padding_y) {
        // Grow using padding only; never increase the font size.
        adjusted_padding_y = (target_height - font_size) * 0.5f;
    } else {
        // Shrink padding first, then the font if necessary.
        adjusted_padding_y =
            std::min(original_padding_y,
                     std::max(0.0f, (target_height - font_size) * 0.5f));
        adjusted_font_size =
            std::max(1.0f, target_height - 2.0f * adjusted_padding_y);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x, adjusted_padding_y));
    g.FontSize = adjusted_font_size;
    ImGui::SetNextItemWidth(size.x);
    utl::scope_guard end = [&] {
        ImGui::PopStyleVar();
        g.FontSize = font_size;
    };
    return std::invoke(std::forward<Callback>(callback));
}

// =============================================================================
// Text
// =============================================================================

namespace {

struct TextView : View {
    explicit TextView(Property<std::string const>&& text, TextOptions options)
        : View({SizeSpec::fit(), SizeSpec::fit()})
        , options(std::move(options))
        , text(std::move(text)) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    TextOptions options;
    Property<std::string const> text;
};

} // namespace

void TextView::do_display(Rect frame) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    auto text = this->text.get();
    float wrap_width = options.wrap ? frame.size.x : 0.0f;
    wrap_width = std::min(wrap_width, options.max_wrap_width);
    auto const& text_ref = text.value();

    bool const use_child_window = options.wrap && options.scrollable_on_clip;

    vml::float2 pos = frame.origin;
    if (use_child_window) {
        ImGui::PushID(this);
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarPadding, 0.0f);
        ImGui::BeginChild("##wrapping-text-view", frame.size);
        ImGui::PopStyleVar(2);
        vml::float2 text_size = ImGui::CalcTextSize(
            text_ref.data(), text_ref.data() + text_ref.size(), false,
            wrap_width);
        ImGui::ItemSize(text_size);
        ImGui::ItemAdd({frame.origin, frame.origin + text_size}, 0);
        pos.y -= ImGui::GetScrollY();
        ImGui::PushClipRect(frame.min(), frame.max(), true);
    } else {
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::ItemSize(frame.size);
        ImGui::ItemAdd({frame.min(), frame.max()}, 0);
        ImGui::PushClipRect(frame.min(), frame.max(), true);
    }
    vml::float2 padding = [&] -> vml::float2 {
        if (options.is_label) return ImGui::GetStyle().FramePadding;
        return {0.0f, 0.0f};
    }();
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddText(nullptr, 0.0f, pos + padding, ImGui::GetColorU32(ImGuiCol_Text),
                text_ref.data(), text_ref.data() + text_ref.size(), wrap_width);
    if (use_child_window) {
        ImGui::EndChild();
        ImGui::PopID();
    } else {
        ImGui::PopClipRect();
    }
}

vml::float2 TextView::do_calc_intrinsic_size(vml::float2 max_size) const {
    auto text = this->text.get();
    auto const& text_ref = text.value();
    float wrap_width = !options.wrap       ? -1.0f
                       : max_size.x < 0.0f ? options.default_wrap_width
                                           : max_size.x;
    wrap_width = std::min(wrap_width, options.max_wrap_width);
    SizeSpec width_spec = size_spec().x;
    if (width_spec.is_fixed())
        wrap_width = std::min(wrap_width, width_spec.fixed_size());
    vml::float2 text_size = ImGui::CalcTextSize(
        text_ref.data(), text_ref.data() + text_ref.size(), false, wrap_width);
    if (options.is_label) {
        vml::float2 padding = ImGui::GetStyle().FramePadding;
        return text_size + 2.0f * padding;
    }
    return text_size;
}

Owner<View> ui::text(Property<std::string const> text_arg,
                     TextOptions options) {
    return allocate<TextView>(std::move(text_arg), std::move(options));
}

Owner<View> ui::label(Property<std::string const> text_arg) {
    return text(std::move(text_arg), {.wrap = false, .is_label = true});
}

// =============================================================================
// TextInput
// =============================================================================

namespace {

// This is kept deliberately simple for now. If we need a text callback later,
// we can still add that
struct TextInputView : View {
    explicit TextInputView(std::string* buffer, TextInputOptions options)
        : View({SizeSpec::fill(), SizeSpec::fit()})
        , options(std::move(options))
        , buffer(buffer) {
        assert(options.num_lines >= 1);
    }

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    TextInputOptions options;
    std::string* buffer;
};

} // namespace

void TextInputView::do_display(Rect frame) {
    assert(options.num_lines >= 1);
    ImGui::PushID(this);
    ImGui::SetCursorScreenPos(frame.origin);
    push_control_colors();
    if (options.num_lines == 1) {
        ImGui::SetNextItemWidth(frame.size.x);
        ImGui::InputText("##text-input", buffer);
    } else {
        ImGui::InputTextMultiline("##text-input", buffer, frame.size);
    }
    pop_control_colors();
    ImGui::PopID();
}

vml::float2 TextInputView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {default_control_width(), options.num_lines * ImGui::GetFontSize() +
                                         2.0f * style.FramePadding.y};
}

Owner<View> ui::text_input(std::string* buffer, TextInputOptions options) {
    return allocate<TextInputView>(buffer, std::move(options));
}

static std::array<char, 5> get_code_point_utf8(unsigned code_point) {
    std::array<char, 5> buf;
    ImTextCharToUtf8(buf.data(), code_point);
    return buf;
}

// =============================================================================
// Icon
// =============================================================================

namespace {

struct IconView : View {
    explicit IconView(Property<Icon const> icon)
        : View({SizeSpec::fit(), SizeSpec::fit()})
        , icon(std::move(icon)) {}

    void do_display(Rect frame) override {
        FontIcon font_icon = std::get<FontIcon>(icon.materialize());
        auto text = get_code_point_utf8(font_icon.code_point);
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::ItemSize(frame.size);
        ImGui::ItemAdd({frame.min(), frame.max()}, 0);
        ImGui::PushClipRect(frame.min(), frame.max(), true);
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddText(font_icon.font, 0.0f, frame.origin,
                    ImGui::GetColorU32(ImGuiCol_Text), text.data(),
                    text.data() + 4);
        ImGui::PopClipRect();
    }

    vml::float2 do_calc_intrinsic_size(vml::float2) const override {
        FontIcon font_icon = std::get<FontIcon>(icon.materialize());
        auto text = get_code_point_utf8(font_icon.code_point);
        ImGui::PushFont(font_icon.font, 0.0f);
        vml::float2 text_size =
            ImGui::CalcTextSize(text.data(), text.data() + 4);
        ImGui::PopFont();
        return text_size;
    }

    Property<Icon const> icon;
};

} // namespace

Owner<View> ui::icon(Property<Icon const> icon) {
    return allocate<IconView>(std::move(icon));
}

// =============================================================================
// Button
// =============================================================================

namespace {

struct ButtonView : View {
    using Label =
        std::variant<Property<std::string const>, Property<Icon const>>;

    explicit ButtonView(Label&& label, std::function<void()>&& on_click)
        : View({SizeSpec::fit(), SizeSpec::fit()})
        , label(std::move(label))
        , on_click(std::move(on_click)) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Label label;
    std::function<void()> on_click;
};

} // namespace

static bool display_button(Property<std::string const> const& label,
                           vml::float2 size) {
    auto text = label.get();
    return ImGui::ButtonEx(text.value().c_str(), size);
}

static bool display_button(Property<Icon const> const& label,
                           vml::float2 size) {
    FontIcon icon = std::get<FontIcon>(label.materialize());
    ImGui::PushFont(icon.font, 0.0f);
    auto text = get_code_point_utf8(icon.code_point);
    bool clicked = ImGui::ButtonEx(text.data(), size);
    ImGui::PopFont();
    return clicked;
}

void ButtonView::do_display(Rect frame) {
    if (!positive(frame.size)) return;
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    push_control_colors();
    bool clicked = std::visit(
        [&](auto& prop) { return display_button(prop, frame.size); }, label);
    pop_control_colors();
    ImGui::PopID();
    if (clicked && on_click) on_click();
}

static vml::float2 measure_button(Property<std::string const> const& label) {
    auto& style = ImGui::GetStyle();
    auto text = label.get();
    auto& text_ref = text.value();
    vml::float2 text_size = ImGui::CalcTextSize(
        text_ref.data(), text_ref.data() + text_ref.size(), true);
    return text_size + 2.0f * (vml::float2)style.FramePadding;
}

static vml::float2 measure_button(Property<Icon const> const&) {
    auto& style = ImGui::GetStyle();
    float y = ImGui::GetFontSize();
    float x = 1.5f * y;
    return vml::float2{x, y} + 2.0f * (vml::float2)style.FramePadding;
}

vml::float2 ButtonView::do_calc_intrinsic_size(vml::float2) const {
    return std::visit([](auto& prop) { return measure_button(prop); }, label);
}

Owner<View> ui::button(Property<std::string const> label,
                       std::function<void()> on_click) {
    return allocate<ButtonView>(std::move(label), std::move(on_click));
}

Owner<View> ui::button(Property<Icon const> icon,
                       std::function<void()> action) {
    return allocate<ButtonView>(std::move(icon), std::move(action));
}

// =============================================================================
// Checkbox
// =============================================================================

namespace {

struct CheckboxView : View {
    explicit CheckboxView(Property<bool>&& state)
        : View({SizeSpec::fit(), SizeSpec::fit()})
        , state(std::move(state)) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<bool> state;
};

} // namespace

void CheckboxView::do_display(Rect frame) {
    if (!positive(frame.size)) return;
    ImGui::SetCursorScreenPos(frame.origin);
    bool current_value = state.materialize();
    ImGui::PushID(this);
    bool active = ImGui::Checkbox("##checkbox", &current_value);
    ImGui::PopID();
    if (active) state.set(current_value);
}

vml::float2 CheckboxView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return vml::float2(ImGui::GetFontSize()) +
           2.0f * (vml::float2)style.FramePadding;
}

Owner<View> ui::checkbox(Property<bool> state) {
    return allocate<CheckboxView>(std::move(state));
}

// =============================================================================
// Combo
// =============================================================================

namespace {

struct ComboView : View {
    ComboView(Property<int>&& selection, Property<std::string const>&& preview,
              Property<std::vector<std::string> const>&& items,
              ComboOptions options)
        : View({SizeSpec::fill(), SizeSpec::fit()})
        , selection(std::move(selection))
        , preview(std::move(preview))
        , items(std::move(items))
        , options(options) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<int> selection;
    Property<std::string const> preview;
    Property<std::vector<std::string> const> items;
    ComboOptions options;
};

} // namespace

static int translate_combo_flags(ComboOptions const& options) {
    int flags = 0;
    switch (options.height) {
    case ComboHeight::small:
        flags |= ImGuiComboFlags_HeightSmall;
        break;
    case ComboHeight::regular:
        flags |= ImGuiComboFlags_HeightRegular;
        break;
    case ComboHeight::large:
        flags |= ImGuiComboFlags_HeightLarge;
        break;
    case ComboHeight::giant:
        flags |= ImGuiComboFlags_HeightLargest;
        break;
    }
    if (!options.arrow_button) flags |= ImGuiComboFlags_NoArrowButton;
    return flags;
}

void ComboView::do_display(Rect frame) {
    ImGui::PushID(this);
    with_widget_size(frame.size, [&] {
        ImGui::SetCursorScreenPos(frame.origin);
        auto preview = this->preview.get();
        auto& style = ImGui::GetStyle();
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding,
                             2.0f * style.FramePadding.x);
        int combo_flags = translate_combo_flags(options);
        push_control_colors();
        bool open =
            ImGui::BeginCombo("##combo", preview.value().c_str(), combo_flags);
        pop_control_colors();
        ImGui::PopStyleVar();
        if (open) {
            int selection = this->selection.materialize();
            int new_selection = -1;
            auto options = this->items.get();
            int index = 0;
            for (std::string const& option : options.value()) {
                ImGui::PushID(index);
                if (ImGui::Selectable(option.c_str(), index == selection))
                    new_selection = index;
                ImGui::PopID();
                ++index;
            }
            if (new_selection != -1) this->selection.set(new_selection);
            ImGui::EndCombo();
        }
    });
    ImGui::PopID();
}

vml::float2 ComboView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {default_control_width(),
            ImGui::GetFontSize() + 2.0f * style.FramePadding.y};
}

Owner<View> ui::combo(Property<int> selection,
                      Property<std::string const> preview,
                      Property<std::vector<std::string> const> items,
                      ComboOptions options) {
    return allocate<ComboView>(std::move(selection), std::move(preview),
                               std::move(items), options);
}

// =============================================================================
// Slider
// =============================================================================

namespace {

template <typename T> struct SliderView : View {
    using ScalarType = ui::ScalarType<T>;

    explicit SliderView(Property<T>&& state, Property<ScalarType const>&& min,
                        Property<ScalarType const>&& max,
                        SliderOptions options = {});

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<T> state;
    Property<ScalarType const> min;
    Property<ScalarType const> max;
    SliderOptions options;
};

} // namespace

template <typename T>
SliderView<T>::SliderView(Property<T>&& state, Property<ScalarType const>&& min,
                          Property<ScalarType const>&& max,
                          SliderOptions options)
    : View({SizeSpec::fill(1.0f, FillBasis::intrinsic), SizeSpec::fit()})
    , state(std::move(state))
    , min(std::move(min))
    , max(std::move(max))
    , options(options) {}

template <typename T> static constexpr ImGuiDataType get_data_type_impl() {
#define MAP(Type, Result)                                                      \
    else if constexpr (std::is_same_v<T, Type>) return Result;

    if constexpr (false) return 0;
    MAP(std::int8_t, ImGuiDataType_S8)
    MAP(std::int16_t, ImGuiDataType_S16)
    MAP(std::int32_t, ImGuiDataType_S32)
    MAP(std::int64_t, ImGuiDataType_S64)
    MAP(std::uint8_t, ImGuiDataType_U8)
    MAP(std::uint16_t, ImGuiDataType_U16)
    MAP(std::uint32_t, ImGuiDataType_U32)
    MAP(std::uint64_t, ImGuiDataType_U64)
    MAP(float, ImGuiDataType_Float)
    MAP(double, ImGuiDataType_Double)
    else static_assert(!std::is_same_v<T, T>, "Unsupported type");

#undef MAP
}

template <typename T> static constexpr ImGuiDataType get_data_type() {
    if constexpr (std::is_arithmetic_v<T>) return get_data_type_impl<T>();
    else if constexpr (vml::is_vector<T>::value)
        return get_data_type_impl<typename vml::get_underlying_type<T>::type>();
    else static_assert(!std::is_same_v<T, T>, "Unsupported type");
}

template <typename T> static constexpr unsigned get_data_dimension() {
    if constexpr (std::is_arithmetic_v<T>) return 1;
    else if constexpr (vml::is_vector<T>::value)
        return vml::get_vector_size<T>::value;
    else static_assert(!std::is_same_v<T, T>, "Unsupported type");
}

static int translate_slider_flags(SliderOptions const& options) {
    int result = 0;
    if (options.logarithmic) result |= ImGuiSliderFlags_Logarithmic;
    if (!options.round_to_format) result |= ImGuiSliderFlags_NoRoundToFormat;
    if (!options.input) result |= ImGuiSliderFlags_NoInput;
#if 0 // Not allowed for sliders (only for drag)
    if (options.wrap_around) result |= ImGuiSliderFlags_WrapAround;
#endif
    if (options.clamp_on_input) result |= ImGuiSliderFlags_ClampOnInput;
    if (options.clamp_zero_range) result |= ImGuiSliderFlags_ClampZeroRange;
    if (!options.speed_tweaks) result |= ImGuiSliderFlags_NoSpeedTweaks;
    if (options.color_markers) result |= ImGuiSliderFlags_ColorMarkers;
    return result;
}

template <typename T> void SliderView<T>::do_display(Rect frame) {
    if (!non_negative(frame.size)) return;
    static constexpr unsigned num_components = get_data_dimension<T>();
    static_assert(num_components >= 1);
    static_assert(num_components <= 4);
    T value = state.materialize();
    ScalarType const min = this->min.materialize();
    ScalarType const max = this->max.materialize();
    push_control_colors();
    bool changed = with_widget_size(frame.size, [&] {
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::PushID(this);
        int flags = translate_slider_flags(options);
        utl::scope_guard end = [&] { ImGui::PopID(); };
        ImGuiDataType data_type = get_data_type<T>();
        if constexpr (num_components == 1)
            return ImGui::SliderScalar("##slider", data_type, &value, &min,
                                       &max, options.format, flags);
        else
            return ImGui::SliderScalarN("##slider", data_type, value.data(),
                                        num_components, &min, &max,
                                        options.format, flags);
    });
    pop_control_colors();
    if (changed) state.set(value);
}

template <typename T>
vml::float2 SliderView<T>::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {default_control_width(),
            ImGui::GetFontSize() + 2.0f * style.FramePadding.y};
}

template <typename T>
Owner<View> ui::slider(Property<T> state, Property<ScalarType<T> const> min,
                       Property<ScalarType<T> const> max,
                       SliderOptions options) {
    return allocate<SliderView<T>>(std::move(state), std::move(min),
                                   std::move(max), std::move(options));
}

#define INSTANTIATE_SLIDER(...)                                                \
    template Owner<View> ui::slider<__VA_ARGS__>(                              \
        Property<__VA_ARGS__> state,                                           \
        Property<ScalarType<__VA_ARGS__> const> min,                           \
        Property<ScalarType<__VA_ARGS__> const> max, SliderOptions options);

INSTANTIATE_SLIDER(std::int8_t)
INSTANTIATE_SLIDER(std::int16_t)
INSTANTIATE_SLIDER(std::int32_t)
INSTANTIATE_SLIDER(std::int64_t)
INSTANTIATE_SLIDER(std::uint8_t)
INSTANTIATE_SLIDER(std::uint16_t)
INSTANTIATE_SLIDER(std::uint32_t)
INSTANTIATE_SLIDER(std::uint64_t)
INSTANTIATE_SLIDER(float)
INSTANTIATE_SLIDER(double)
INSTANTIATE_SLIDER(vml::vector<std::int8_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::int16_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::int32_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::int64_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::uint8_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::uint16_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::uint32_t, 2>)
INSTANTIATE_SLIDER(vml::vector<std::uint64_t, 2>)
INSTANTIATE_SLIDER(vml::vector<float, 2>)
INSTANTIATE_SLIDER(vml::vector<double, 2>)
INSTANTIATE_SLIDER(vml::vector<std::int8_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::int16_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::int32_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::int64_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::uint8_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::uint16_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::uint32_t, 3>)
INSTANTIATE_SLIDER(vml::vector<std::uint64_t, 3>)
INSTANTIATE_SLIDER(vml::vector<float, 3>)
INSTANTIATE_SLIDER(vml::vector<double, 3>)
INSTANTIATE_SLIDER(vml::vector<std::int8_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::int16_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::int32_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::int64_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::uint8_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::uint16_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::uint32_t, 4>)
INSTANTIATE_SLIDER(vml::vector<std::uint64_t, 4>)
INSTANTIATE_SLIDER(vml::vector<float, 4>)
INSTANTIATE_SLIDER(vml::vector<double, 4>)

// =============================================================================
// ColorEdit
// =============================================================================

namespace {

template <std::size_t NumComponents> struct ColorEditView : View {
    using ColorType = vml::vector<float, NumComponents>;

    explicit ColorEditView(Property<ColorType>&& state,
                           ColorEditOptions options)
        : View({SizeSpec::fill(), SizeSpec::fit()})
        , state(std::move(state))
        , options(std::move(options)) {}

    Property<ColorType> const& get_state() const { return state; }

    void set_state(Property<ColorType> state) { state = std::move(state); }

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Property<ColorType> state;
    ColorEditOptions options;
};

} // namespace

static int translate_color_edit_flags(ColorEditOptions const& options) {
    int flags = 0;
    if (!options.alpha) flags |= ImGuiColorEditFlags_NoAlpha;
    if (!options.picker) flags |= ImGuiColorEditFlags_NoPicker;
    if (!options.options) flags |= ImGuiColorEditFlags_NoOptions;
    if (!options.small_preview) flags |= ImGuiColorEditFlags_NoSmallPreview;
    if (!options.inputs) flags |= ImGuiColorEditFlags_NoInputs;
    if (!options.tooltip) flags |= ImGuiColorEditFlags_NoTooltip;
    if (!options.enable_drag_drop) flags |= ImGuiColorEditFlags_NoDragDrop;
    return flags;
}

template <std::size_t N> void ColorEditView<N>::do_display(Rect frame) {
    if (!non_negative(frame.size)) return;
    ColorType value = state.materialize();
    push_control_colors();
    bool changed = with_widget_size(frame.size, [&] {
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::PushID(this);
        int flags = translate_color_edit_flags(options);
        utl::scope_guard end = [&] { ImGui::PopID(); };
        if constexpr (N == 3)
            return ImGui::ColorEdit3("##color-edit", value.data(), flags);
        else return ImGui::ColorEdit4("##color-edit", value.data(), flags);
    });
    pop_control_colors();
    if (changed) state.set(value);
}

template <std::size_t N>
vml::float2 ColorEditView<N>::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {default_control_width(),
            ImGui::GetFontSize() + 2.0f * style.FramePadding.y};
}

Owner<View> ui::color_edit(Property<vml::float3> state,
                           ColorEditOptions options) {
    return allocate<ColorEditView<3>>(std::move(state), std::move(options));
}

Owner<View> ui::color_edit(Property<vml::float4> state,
                           ColorEditOptions options) {
    return allocate<ColorEditView<4>>(std::move(state), std::move(options));
}

static SizeSpec2 separator_size_req(Axis main_axis) {
    switch (main_axis) {
    case Axis::horizontal:
        return {SizeSpec::fill(), SizeSpec::fit()};
    case Axis::vertical:
        return {SizeSpec::fit(), SizeSpec::fill()};
    }
}

// =============================================================================
// Separator
// =============================================================================

namespace {

struct SeparatorView : View {
    explicit SeparatorView(Axis main_axis)
        : View(separator_size_req(main_axis))
        , main_axis(main_axis) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    /// The axis along which the line is drawn, that is `vertical` for a
    /// vertical line (between horizontally adjacent elements) and `horizontal`
    /// for a horizontal line (between vertically adjacent elements).
    ///
    ///     `vertical`   -> `vseparator()`
    ///     `horizontal` -> `hseparator()`
    ///
    /// Note that in a `vstack` you would normally use `hseparator()` to
    /// separate the elements and vice-versa.
    Axis main_axis;
};

} // namespace

void SeparatorView::do_display(Rect frame) {
    if (!positive(frame.size)) return;

    auto& style = ImGui::GetStyle();
    auto* DL = ImGui::GetWindowDrawList();
    switch (main_axis) {
    case Axis::horizontal:
        DL->AddLineH(
            frame.min().x, frame.max().x, frame.origin.y + 0.5f * frame.size.y,
            ImGui::GetColorU32(ImGuiCol_Separator), style.SeparatorSize);
        break;
    case Axis::vertical:
        DL->AddLineV(frame.origin.x + 0.5f * frame.size.x, frame.min().y,
                     frame.max().y, ImGui::GetColorU32(ImGuiCol_Separator),
                     style.SeparatorSize);
        break;
    }
}

vml::float2 SeparatorView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    switch (main_axis) {
    case Axis::horizontal:
        return {0.0f, style.SeparatorSize + 2.0f * style.ItemSpacing.y};
    case Axis::vertical:
        return {style.SeparatorSize + 2.0f * style.ItemSpacing.x, 0.0f};
    }
}

Owner<View> ui::hseparator() {
    return allocate<SeparatorView>(Axis::horizontal);
}

Owner<View> ui::vseparator() { return allocate<SeparatorView>(Axis::vertical); }

// =============================================================================
// Spacer
// =============================================================================

namespace {

struct SpacerView : View {
    SpacerView(float weight = 1.0f)
        : View({SizeSpec::fill(weight), SizeSpec::fill(weight)}) {}
    void do_display(Rect) override {}
    vml::float2 do_calc_intrinsic_size(vml::float2) const override {
        return {0.0f, 0.0f};
    }
};

} // namespace

Owner<View> ui::spacer(float weight) { return allocate<SpacerView>(weight); }

// =============================================================================
// Image
// =============================================================================

namespace {

struct ImageView : View {
    ImageView(void const* image, float aspect_ratio)
        : View({SizeSpec::fill(), SizeSpec::fit()})
        , m_image(image)
        , m_aspect_ratio(aspect_ratio) {}

    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    void const* m_image;
    float m_aspect_ratio;
};

} // namespace

static bool is_valid_ratio(float ratio) {
    return ratio > 0.0f && !std::isinf(ratio) && !std::isnan(ratio);
}

void ImageView::do_display(Rect frame) {
    if (!is_valid_ratio(m_aspect_ratio)) return;
    if (frame.size.x <= 0.0f || frame.size.y <= 0.0f) return;
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::Image(reinterpret_cast<std::uintptr_t>(m_image), frame.size);
    return;
}

vml::float2 ImageView::do_calc_intrinsic_size(vml::float2 max_size) const {
    if (!is_valid_ratio(m_aspect_ratio)) return {};
    float const default_width = 400.0f;
    float const default_height = default_width / m_aspect_ratio;
    if (max_size.x < 0.0f && max_size.y < 0.0f)
        max_size = {default_width, default_height};
    else if (max_size.x < 0.0f) max_size.x = max_size.y * m_aspect_ratio;
    else if (max_size.y < 0.0f) max_size.y = max_size.x / m_aspect_ratio;
    float width = max_size.x;
    float height = width / m_aspect_ratio;
    if (height > max_size.y) {
        height = max_size.y;
        width = height * m_aspect_ratio;
    }
    return {width, height};
}

Owner<View> ui::image(void const* image, float aspect_ratio) {
    return allocate<ImageView>(image, aspect_ratio);
}
