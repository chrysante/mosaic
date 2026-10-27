#pragma once

#include <cfloat>
#include <optional>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// StackView
// =============================================================================

class StackView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fit(),
                                                          SizeReq::fit()};

    explicit StackView(Orientation orientation, ViewContainer<>&& children,
                       std::optional<Alignment> align_override = std::nullopt);

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Orientation m_orientation;
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
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit ScrollView(Orientation orientation, ViewContainer<>&& children);

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    Orientation m_orientation;
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
    Property<std::string const> title;
    Owner<View> content;
};

inline Tab tab(Property<std::string const> title, Owner<View> content) {
    return {std::move(title), std::move(content)};
}

class TabView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit TabView(Array<Tab>&& tabs);

    Array<Tab>& tabs() { return m_tabs; }
    Array<Tab> const& tabs() const { return m_tabs; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

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
    Owner<View> content;
    float min_width = 100.0f;
    float max_width = FLT_MAX;
};

inline Column column(Owner<View> content, float min_width = 100.0f,
                     float max_width = FLT_MAX) {
    return Column{std::move(content), min_width, max_width};
}

class ColumnView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit ColumnView(Array<Column>&& columns);

    Array<Column>& columns() { return m_columns; }
    Array<Column> const& columns() const { return m_columns; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

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
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fit(),
                                                          SizeReq::fit()};

    explicit SwitchView(Property<Index> index, ViewContainer<>&& children);

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

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