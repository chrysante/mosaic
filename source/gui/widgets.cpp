#include "gui/widgets.h"

#include <cmath>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>
#include <type_traits>

#include "gui/view_support.h"
#include "utl/scope_guard.hpp"
#include "vml/common.hpp"

using namespace mosaic;
using namespace ui;

static bool positive(vml::float2 size) {
    return size.map([](auto v) { return v > 0.0f; }).all();
}

static bool non_negative(vml::float2 size) {
    return size.map([](auto v) { return v >= 0.0f; }).all();
}

TextView::TextView(Property<std::string const>&& text, TextOptions options)
    : View({SizeReq::fit(), SizeReq::fit()})
    , m_options(std::move(options))
    , m_text(std::move(text)) {}

void TextView::do_display(Rect frame) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    auto text = m_text.get();
    float wrap_width = m_options.wrap ? frame.size.x : 0.0f;
    wrap_width = std::min(wrap_width, m_options.max_wrap_width);
    auto const& text_ref = text.value();

    bool const use_child_window =
        m_options.wrap && m_options.scrollable_on_clip;

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
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddText(nullptr, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text),
                text_ref.data(), text_ref.data() + text_ref.size(), wrap_width);
    if (use_child_window) {
        ImGui::EndChild();
        ImGui::PopID();
    } else {
        ImGui::PopClipRect();
    }
}

vml::float2 TextView::do_calc_intrinsic_size(vml::float2 max_size) const {
    auto text = m_text.get();
    auto const& text_ref = text.value();
    float wrap_width = !m_options.wrap     ? -1.0f
                       : max_size.x < 0.0f ? m_options.default_wrap_width
                                           : max_size.x;
    wrap_width = std::min(wrap_width, m_options.max_wrap_width);
    SizeReq width_req = size_requirement().x;
    if (width_req.is_fixed())
        wrap_width = std::min(wrap_width, width_req.fixed_size());
    return ImGui::CalcTextSize(
        text_ref.data(), text_ref.data() + text_ref.size(), false, wrap_width);
}

Owner<TextView> ui::text(Property<std::string const> text_arg,
                         TextOptions options) {
    return allocate<TextView>(std::move(text_arg), std::move(options));
}

ButtonView::ButtonView(Property<std::string const>&& label,
                       std::function<void()>&& on_click)
    : View({SizeReq::fit(), SizeReq::fit()})
    , m_label(std::move(label))
    , m_on_click(std::move(on_click)) {}

void ButtonView::do_display(Rect frame) {
    if (!positive(frame.size)) return;
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    auto label = m_label.get();
    bool clicked = ImGui::ButtonEx(label.value().c_str(), frame.size);
    ImGui::PopID();
    if (clicked && m_on_click) m_on_click();
}

vml::float2 ButtonView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    auto label = m_label.get();
    auto& label_ref = label.value();
    vml::float2 text_size = ImGui::CalcTextSize(
        label_ref.data(), label_ref.data() + label_ref.size(), true);
    return text_size + 2.0f * (vml::float2)style.FramePadding;
}

Owner<ButtonView> ui::button(Property<std::string const> label,
                             std::function<void()> on_click) {
    return allocate<ButtonView>(std::move(label), std::move(on_click));
}

CheckboxView::CheckboxView(Property<bool>&& state)
    : View({SizeReq::fit(), SizeReq::fit()})
    , m_state(std::move(state)) {}

void CheckboxView::do_display(Rect frame) {
    if (!positive(frame.size)) return;
    ImGui::SetCursorScreenPos(frame.origin);
    bool current_value = m_state.materialize();
    ImGui::PushID(this);
    bool active = ImGui::Checkbox("##checkbox", &current_value);
    ImGui::PopID();
    if (active) m_state.set(current_value);
}

vml::float2 CheckboxView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return vml::float2(ImGui::GetFontSize()) +
           2.0f * (vml::float2)style.FramePadding;
}

Owner<CheckboxView> ui::checkbox(Property<bool> state) {
    return allocate<CheckboxView>(std::move(state));
}

template <typename T>
SliderView<T>::SliderView(Property<T>&& state, Property<ScalarType const>&& min,
                          Property<ScalarType const>&& max,
                          SliderOptions options)
    : View({SizeReq::fill(), SizeReq::fit()})
    , m_state(std::move(state))
    , m_min(std::move(min))
    , m_max(std::move(max))
    , m_options(options) {}

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
    if (options.wrap_around) result |= ImGuiSliderFlags_WrapAround;
    if (options.clamp_on_input) result |= ImGuiSliderFlags_ClampOnInput;
    if (options.clamp_zero_range) result |= ImGuiSliderFlags_ClampZeroRange;
    if (!options.speed_tweaks) result |= ImGuiSliderFlags_NoSpeedTweaks;
    if (options.color_markers) result |= ImGuiSliderFlags_ColorMarkers;
    return result;
}

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

template <typename T> void SliderView<T>::do_display(Rect frame) {
    if (!non_negative(frame.size)) return;
    static constexpr unsigned num_components = get_data_dimension<T>();
    static_assert(num_components >= 1);
    static_assert(num_components <= 4);
    T value = m_state.materialize();
    ScalarType const min = m_min.materialize();
    ScalarType const max = m_max.materialize();
    bool changed = with_widget_size(frame.size, [&] {
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::PushID(this);
        int flags = translate_slider_flags(m_options);
        utl::scope_guard end = [&] { ImGui::PopID(); };
        if constexpr (num_components == 1)
            return ImGui::SliderScalar("##slider", get_data_type<T>(), &value,
                                       &min, &max, m_options.format.c_str(),
                                       flags);
        else
            return ImGui::SliderScalarN("##slider", get_data_type<T>(),
                                        value.data(), num_components, &min,
                                        &max, m_options.format.c_str(), flags);
    });
    if (changed) m_state.set(value);
}

template <typename T>
vml::float2 SliderView<T>::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {300.0f, ImGui::GetFontSize() + 2.0f * style.FramePadding.y};
}

template class ui::SliderView<std::int8_t>;
template class ui::SliderView<std::int16_t>;
template class ui::SliderView<std::int32_t>;
template class ui::SliderView<std::int64_t>;
template class ui::SliderView<std::uint8_t>;
template class ui::SliderView<std::uint16_t>;
template class ui::SliderView<std::uint32_t>;
template class ui::SliderView<std::uint64_t>;
template class ui::SliderView<float>;
template class ui::SliderView<double>;

template class ui::SliderView<vml::vector<std::int8_t, 2>>;
template class ui::SliderView<vml::vector<std::int16_t, 2>>;
template class ui::SliderView<vml::vector<std::int32_t, 2>>;
template class ui::SliderView<vml::vector<std::int64_t, 2>>;
template class ui::SliderView<vml::vector<std::uint8_t, 2>>;
template class ui::SliderView<vml::vector<std::uint16_t, 2>>;
template class ui::SliderView<vml::vector<std::uint32_t, 2>>;
template class ui::SliderView<vml::vector<std::uint64_t, 2>>;
template class ui::SliderView<vml::vector<float, 2>>;
template class ui::SliderView<vml::vector<double, 2>>;

template class ui::SliderView<vml::vector<std::int8_t, 3>>;
template class ui::SliderView<vml::vector<std::int16_t, 3>>;
template class ui::SliderView<vml::vector<std::int32_t, 3>>;
template class ui::SliderView<vml::vector<std::int64_t, 3>>;
template class ui::SliderView<vml::vector<std::uint8_t, 3>>;
template class ui::SliderView<vml::vector<std::uint16_t, 3>>;
template class ui::SliderView<vml::vector<std::uint32_t, 3>>;
template class ui::SliderView<vml::vector<std::uint64_t, 3>>;
template class ui::SliderView<vml::vector<float, 3>>;
template class ui::SliderView<vml::vector<double, 3>>;

template class ui::SliderView<vml::vector<std::int8_t, 4>>;
template class ui::SliderView<vml::vector<std::int16_t, 4>>;
template class ui::SliderView<vml::vector<std::int32_t, 4>>;
template class ui::SliderView<vml::vector<std::int64_t, 4>>;
template class ui::SliderView<vml::vector<std::uint8_t, 4>>;
template class ui::SliderView<vml::vector<std::uint16_t, 4>>;
template class ui::SliderView<vml::vector<std::uint32_t, 4>>;
template class ui::SliderView<vml::vector<std::uint64_t, 4>>;
template class ui::SliderView<vml::vector<float, 4>>;
template class ui::SliderView<vml::vector<double, 4>>;

template <std::size_t N>
ColorEditView<N>::ColorEditView(Property<ColorType>&& state,
                                ColorEditOptions options)
    : View({SizeReq::fill(), SizeReq::fit()})
    , m_state(std::move(state))
    , m_options(std::move(options)) {}

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
    ColorType value = m_state.materialize();
    bool changed = with_widget_size(frame.size, [&] {
        ImGui::SetCursorScreenPos(frame.origin);
        ImGui::PushID(this);
        int flags = translate_color_edit_flags(m_options);
        utl::scope_guard end = [&] { ImGui::PopID(); };
        if constexpr (N == 3)
            return ImGui::ColorEdit3("##color-edit", value.data(), flags);
        else return ImGui::ColorEdit4("##color-edit", value.data(), flags);
    });
    if (changed) m_state.set(value);
}

template <std::size_t N>
vml::float2 ColorEditView<N>::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    return {300.0f, ImGui::GetFontSize() + 2.0f * style.FramePadding.y};
}

template class ui::ColorEditView<3>;
template class ui::ColorEditView<4>;

Owner<ColorEditView<3>> ui::color_edit(Property<vml::float3> state,
                                       ColorEditOptions options) {
    return allocate<ColorEditView<3>>(std::move(state), std::move(options));
}

Owner<ColorEditView<4>> ui::color_edit(Property<vml::float4> state,
                                       ColorEditOptions options) {
    return allocate<ColorEditView<4>>(std::move(state), std::move(options));
}

static SizeReq2 separator_size_req(Axis main_axis) {
    switch (main_axis) {
    case Axis::horizontal:
        return {SizeReq::fill(), SizeReq::fit()};
    case Axis::vertical:
        return {SizeReq::fit(), SizeReq::fill()};
    }
}

SeparatorView::SeparatorView(Axis main_axis)
    : View(separator_size_req(main_axis))
    , m_main_axis(main_axis) {}

void SeparatorView::do_display(Rect frame) {
    if (!positive(frame.size)) return;

    auto& style = ImGui::GetStyle();
    auto* DL = ImGui::GetWindowDrawList();
    switch (m_main_axis) {
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
    switch (m_main_axis) {
    case Axis::horizontal:
        return {0.0f, style.SeparatorSize + 2.0f * style.ItemSpacing.y};
    case Axis::vertical:
        return {style.SeparatorSize + 2.0f * style.ItemSpacing.x, 0.0f};
    }
}

Owner<SeparatorView> ui::hseparator() {
    return allocate<SeparatorView>(Axis::horizontal);
}

Owner<SeparatorView> ui::vseparator() {
    return allocate<SeparatorView>(Axis::vertical);
}

SpacerView::SpacerView(float weight)
    : View({SizeReq::fill(weight), SizeReq::fill(weight)}) {}

void SpacerView::do_display(Rect) {}

vml::float2 SpacerView::do_calc_intrinsic_size(vml::float2) const {
    return {0.0f, 0.0f};
}

Owner<SpacerView> ui::spacer(float weight) {
    return allocate<SpacerView>(weight);
}

ImageView::ImageView(void const* image, float aspect_ratio)
    : View({SizeReq::fill(), SizeReq::fit()})
    , m_image(image)
    , m_aspect_ratio(aspect_ratio) {}

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

Owner<ImageView> ui::image(void const* image, float aspect_ratio) {
    return allocate<ImageView>(image, aspect_ratio);
}
