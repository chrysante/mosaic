#include "gui/widgets.h"

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>
#include <type_traits>

#include "gui/view_support.h"
#include "vml/common.hpp"

using namespace mosaic;
using namespace ui;

static bool positive(vml::float2 size) {
    return size.map([](auto v) { return v > 0.0f; }).all();
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
    auto const& text_ref = text.get();

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
    auto const& text_ref = text.get();
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

Ref<TextView> ui::text(Property<std::string const> text_arg,
                       TextOptions options) {
    return create<TextView>(std::move(text_arg), std::move(options));
}

ButtonView::ButtonView(std::string&& label, std::function<void()>&& on_click)
    : View({SizeReq::fit(), SizeReq::fit()})
    , m_label(std::move(label))
    , m_on_click(std::move(on_click)) {}

void ButtonView::do_display(Rect frame) {
    if (!positive(frame.size)) return;
    ImGui::SetCursorScreenPos(frame.origin);
    if (ImGui::ButtonEx(m_label.c_str(), frame.size) && m_on_click)
        m_on_click();
}

vml::float2 ButtonView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    vml::float2 text_size = ImGui::CalcTextSize(
        m_label.data(), m_label.data() + m_label.size(), true);
    return text_size + 2.0f * (vml::float2)style.FramePadding;
}

Ref<ButtonView> ui::button(std::string label, std::function<void()> on_click) {
    return create<ButtonView>(std::move(label), std::move(on_click));
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

Ref<CheckboxView> ui::checkbox(Property<bool> state) {
    return create<CheckboxView>(std::move(state));
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
    if (options.speed_tweaks) result |= ImGuiSliderFlags_NoSpeedTweaks;
    if (options.color_markers) result |= ImGuiSliderFlags_ColorMarkers;
    return result;
}

template <typename T> void SliderView<T>::do_display(Rect frame) {
    if (!positive(frame.size)) return;

    static constexpr unsigned num_components = get_data_dimension<T>();
    static_assert(num_components >= 1);
    static_assert(num_components <= 4);

    T value = m_state.materialize();
    ScalarType const min = m_min.materialize();
    ScalarType const max = m_max.materialize();

    // Hacky way to adjust the height of the widget
    ImGuiStyle const& style = ImGui::GetStyle();
    float const font_size = ImGui::GetFontSize();
    float const original_padding_y = style.FramePadding.y;
    float const target_height = frame.size.y;
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
    GImGui->FontSize = adjusted_font_size;
    ImGui::SetNextItemWidth(frame.size.x);
    ImGui::SetCursorScreenPos(frame.origin);
    ImGui::PushID(this);
    int flags = translate_slider_flags(m_options);
    bool changed = [&] {
        if constexpr (num_components == 1)
            return ImGui::SliderScalar("##slider", get_data_type<T>(), &value,
                                       &min, &max, m_options.format.c_str(),
                                       flags);
        else
            return ImGui::SliderScalarN("##slider", get_data_type<T>(),
                                        value.data(), num_components, &min,
                                        &max, m_options.format.c_str(), flags);
    }();
    ImGui::PopID();
    ImGui::PopStyleVar();
    GImGui->FontSize = font_size;

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

static SizeReq2 separator_size_req(Orientation orientation) {
    switch (orientation) {
    case Orientation::horizontal:
        return {SizeReq::fill(), SizeReq::fit()};
    case Orientation::vertical:
        return {SizeReq::fit(), SizeReq::fill()};
    }
}

SeparatorView::SeparatorView(Orientation orientation)
    : View(separator_size_req(orientation))
    , m_orientation(orientation) {}

void SeparatorView::do_display(Rect frame) {
    if (!positive(frame.size)) return;

    auto& style = ImGui::GetStyle();
    auto* DL = ImGui::GetWindowDrawList();
    switch (m_orientation) {
    case Orientation::horizontal:
        DL->AddLineH(
            frame.min().x, frame.max().x, frame.origin.y + 0.5f * frame.size.y,
            ImGui::GetColorU32(ImGuiCol_Separator), style.SeparatorSize);
        break;
    case Orientation::vertical:
        DL->AddLineV(frame.origin.x + 0.5f * frame.size.x, frame.min().y,
                     frame.max().y, ImGui::GetColorU32(ImGuiCol_Separator),
                     style.SeparatorSize);
        break;
    }
}

vml::float2 SeparatorView::do_calc_intrinsic_size(vml::float2) const {
    auto& style = ImGui::GetStyle();
    switch (m_orientation) {
    case Orientation::horizontal:
        return {0.0f, style.SeparatorSize + 2.0f * style.ItemSpacing.y};
    case Orientation::vertical:
        return {style.SeparatorSize + 2.0f * style.ItemSpacing.x, 0.0f};
    }
}

Ref<SeparatorView> ui::hseparator() {
    return create<SeparatorView>(Orientation::horizontal);
}

Ref<SeparatorView> ui::vseparator() {
    return create<SeparatorView>(Orientation::vertical);
}

SpacerView::SpacerView(float weight)
    : View({SizeReq::fill(weight), SizeReq::fill(weight)}) {}

void SpacerView::do_display(Rect) {}

vml::float2 SpacerView::do_calc_intrinsic_size(vml::float2) const {
    return {0.0f, 0.0f};
}

Ref<SpacerView> ui::spacer(float weight) { return create<SpacerView>(weight); }
