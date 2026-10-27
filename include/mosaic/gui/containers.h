#pragma once

#include <cfloat>
#include <optional>
#include <vector>

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

Ref<StackView> vstack(ViewContainer<> children,
                      std::optional<Alignment> align_override = std::nullopt);

Ref<StackView> hstack(ViewContainer<> children,
                      std::optional<Alignment> align_override = std::nullopt);

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

Ref<ScrollView> vscroll(ViewContainer<> children);

Ref<ScrollView> hscroll(ViewContainer<> children);

// =============================================================================
// TabView
// =============================================================================

struct Tab {
    Property<std::string const> title;
    Ref<View> content;
};

inline Tab tab(Property<std::string const> title, Ref<View> content) {
    return {std::move(title), std::move(content)};
}

class TabView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit TabView(std::vector<Tab>&& tabs);

    std::vector<Tab>& tabs() { return m_tabs; }
    std::vector<Tab> const& tabs() const { return m_tabs; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    std::vector<Tab> m_tabs;
};

Ref<TabView> tab_view(std::vector<Tab> children);

// =============================================================================
// ColumnView
// =============================================================================

struct Column {
    Ref<View> content;
    float min_width = 100.0f;
    float max_width = FLT_MAX;
};

inline Column column(Ref<View> content, float min_width = 100.0f,
                     float max_width = FLT_MAX) {
    return Column{std::move(content), min_width, max_width};
}

class ColumnView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit ColumnView(std::vector<Column>&& columns);

    std::vector<Column>& columns() { return m_columns; }
    std::vector<Column> const& columns() const { return m_columns; }

private:
    void do_display(Rect frame) override;
    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override;

    std::vector<Column> m_columns;
};

Ref<ColumnView> columns(std::vector<Column> columns);

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

Ref<ConditionalView> if_view(Property<bool> condition, Ref<View> then_view);

Ref<ConditionalView> if_view(Property<bool> condition, Ref<View> then_view,
                             Ref<View> else_view);

Ref<SwitchView<>> switch_view(Property<int> index, ViewContainer<> cases);

} // namespace mosaic::ui