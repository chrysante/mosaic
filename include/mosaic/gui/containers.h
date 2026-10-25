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

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
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

    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    Orientation m_orientation;
    ViewContainer<> m_children;
};

Ref<ScrollView> vscroll(ViewContainer<> children);

Ref<ScrollView> hscroll(ViewContainer<> children);

} // namespace mosaic::ui