#pragma once

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

// =============================================================================
// StackView
// =============================================================================

class StackView : public View {
public:
    enum class Orientation { horizontal = 0, vertical = 1 };

    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit StackView(Orientation orientation, ViewContainer<>&& children,
                       SizeReq2 size_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    Orientation m_orientation;
    ViewContainer<> m_children;
};

Ref<StackView>
vstack(ViewContainer<> children,
       SizeReq2 size_requirement = StackView::default_layout_requirement);

Ref<StackView>
hstack(ViewContainer<> children,
       SizeReq2 size_requirement = StackView::default_layout_requirement);

// =============================================================================
// ScrollView
// =============================================================================

class VScrollView : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fill(),
                                                            SizeReq::fill()};

    explicit VScrollView(ViewContainer<>&& children, SizeReq2 size_requirement);

    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;

    ViewContainer<>& children() { return m_children; }
    ViewContainer<> const& children() const { return m_children; }

private:
    ViewContainer<> m_children;
};

Ref<VScrollView>
vscroll(ViewContainer<> children,
        SizeReq2 size_requirement = VScrollView::default_layout_requirement);

} // namespace mosaic::ui