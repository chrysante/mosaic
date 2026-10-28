#pragma once

#include <cfloat>
#include <cmath>
#include <optional>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// StackView
// =============================================================================

class StackView : public View {
public:
    static constexpr SizeSpec2 default_size_requirement = {SizeSpec::fit(),
                                                           SizeSpec::fit()};

    explicit StackView(Axis main_axis, ViewContainer<>&& children,
                       std::optional<Alignment> align_override = std::nullopt);

    /// The axis along which the content is layed out, i.e. `vertical` for a
    /// `VStack` and `horizontal` for an `HStack`
    Axis main_axis() const { return m_main_axis; }

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Axis m_main_axis;
    std::optional<Alignment> m_align_override;
    ViewContainer<> m_children;
};

Owner<StackView> vstack(ViewContainer<> children,
                        std::optional<Alignment> align_override = std::nullopt);

template <std::size_t N>
Owner<StackView>
vstack(Owner<View> (&&elems)[N],
       std::optional<Alignment> align_override = std::nullopt) {
    return vstack(ViewContainer<>(std::move(elems)), align_override);
}

Owner<StackView> hstack(ViewContainer<> children,
                        std::optional<Alignment> align_override = std::nullopt);

template <std::size_t N>
Owner<StackView>
hstack(Owner<View> (&&elems)[N],
       std::optional<Alignment> align_override = std::nullopt) {
    return hstack(ViewContainer<>(std::move(elems)), align_override);
}

// =============================================================================
// ScrollView
// =============================================================================

class ScrollView : public View {
public:
    static constexpr SizeSpec2 default_size_requirement = {SizeSpec::fill(),
                                                           SizeSpec::fill()};

    explicit ScrollView(Axis main_axis, ViewContainer<>&& children);

    /// The axis along which the content is layed out, i.e. `vertical` for a
    /// `VStack` and `horizontal` for an `HStack`
    Axis main_axis() const { return m_main_axis; }

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;
    bool do_is_leaf() const override { return false; }

    Axis m_main_axis;
    ViewContainer<> m_children;
};

Owner<ScrollView> vscroll(ViewContainer<> children);

template <std::size_t N> Owner<ScrollView> vscroll(Owner<View> (&&elems)[N]) {
    return vscroll(ViewContainer<>(std::move(elems)));
}

Owner<ScrollView> hscroll(ViewContainer<> children);

template <std::size_t N> Owner<ScrollView> hscroll(Owner<View> (&&elems)[N]) {
    return hscroll(ViewContainer<>(std::move(elems)));
}

// =============================================================================
// TabView
// =============================================================================

struct Tab {
    explicit Tab(Property<std::string const> title, Owner<View> content)
        : title(std::move(title))
        , content(std::move(content)) {
        assert(this->content);
    }

    Property<std::string const> title;
    Owner<View> content;
};

inline Tab tab(Property<std::string const> title, Owner<View> content) {
    return Tab(std::move(title), std::move(content));
}

class TabView : public View {
public:
    static constexpr SizeSpec2 default_size_requirement = {SizeSpec::fill(),
                                                           SizeSpec::fill()};

    explicit TabView(Array<Tab>&& tabs);

    Array<Tab>& tabs() { return m_tabs; }
    Array<Tab> const& tabs() const { return m_tabs; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;
    bool do_is_leaf() const override { return false; }

    Array<Tab> m_tabs;
};

Owner<TabView> tab_view(Array<Tab> children);

template <std::size_t N> Owner<TabView> tab_view(Tab (&&children)[N]) {
    return tab_view(Array<Tab>(std::move(children)));
}

// =============================================================================
// ColumnView
// =============================================================================

struct Column {
    explicit Column(Owner<View> content, float min_width, float max_width)
        : content(std::move(content))
        , min_width(min_width)
        , max_width(max_width) {
        assert(this->content);
        assert(std::isfinite(min_width));
        assert(std::isfinite(max_width) || std::isinf(max_width));
        if (min_width < 0.0f) min_width = 0.0f;
        if (min_width > max_width) min_width = max_width;
    }

    Owner<View> content;
    float min_width = 100.0f;
    float max_width = FLT_MAX;
};

inline Column column(Owner<View> content, float min_width = 100.0f,
                     float max_width = FLT_MAX) {
    return Column(std::move(content), min_width, max_width);
}

class ColumnView : public View {
public:
    static constexpr SizeSpec2 default_size_requirement = {SizeSpec::fill(),
                                                           SizeSpec::fill()};

    explicit ColumnView(Array<Column>&& columns);

    Array<Column>& columns() { return m_columns; }
    Array<Column> const& columns() const { return m_columns; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;
    bool do_is_leaf() const override { return false; }

    Array<Column> m_columns;
};

Owner<ColumnView> columns(Array<Column> columns);

template <std::size_t N> Owner<ColumnView> columns(Column (&&columns)[N]) {
    return columns(Array<Column>(std::move(columns)));
}

// =============================================================================
// SwitchView
// =============================================================================

template <typename Index = int> class SwitchView : public View {
public:
    static constexpr SizeSpec2 default_size_requirement = {SizeSpec::fit(),
                                                           SizeSpec::fit()};

    explicit SwitchView(Property<Index> index, ViewContainer<>&& children);

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;
    bool do_is_leaf() const override { return false; }

    Property<Index> m_index;
    ViewContainer<> m_children;
};

using ConditionalView = SwitchView<bool>;

Owner<ConditionalView> if_view(Property<bool> condition, Owner<View> then_view);

Owner<ConditionalView> if_view(Property<bool> condition, Owner<View> then_view,
                               Owner<View> else_view);

Owner<SwitchView<>> switch_view(Property<int> index, ViewContainer<> cases);

template <std::size_t N>
Owner<SwitchView<>> switch_view(Property<int> index, Owner<View> (&&elems)[N]) {
    return switch_view(std::move(index), ViewContainer<>(std::move(elems)));
}

} // namespace mosaic::ui