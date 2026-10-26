#pragma once

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
    vml::float2 calc_intrinsic_size() const override;

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
    vml::float2 calc_intrinsic_size() const override;

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

    std::vector<Tab>& children() { return m_tabs; }
    std::vector<Tab> const& children() const { return m_tabs; }

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    std::vector<Tab> m_tabs;
};

Ref<TabView> tab_view(std::vector<Tab> children);

// =============================================================================
// ColumnView
// =============================================================================

#if 0

class ColumnView : public View {
public:
    static constexpr SizeReq2 default_size_requirement = {SizeReq::fill(),
                                                          SizeReq::fill()};

    explicit ColumnView(ViewContainer<>&& children);

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    void do_display(Rect frame) override;
    vml::float2 calc_intrinsic_size() const override;

    ViewContainer<> m_children;
};

Ref<ColumnView> columns(ViewContainer<> children);

#endif

} // namespace mosaic::ui