#pragma once

#include <vml/vml.hpp>

#include <mosaic/gui/view_support.h>
#include <mosaic/support/export.h>

namespace mosaic::ui {

class MOSAIC_EXPORT View {
public:
    virtual ~View() = default;

    void display(Rect frame);

    // The size the view naturally wants to display its content
    vml::float2 calc_intrinsic_size(vml::float2 max_size) const;

    bool is_leaf() const { return do_is_leaf(); }

    std::reference_wrapper<LayoutSpec> borrow_layout() {
        return std::ref(m_layout.value());
    }

    SizeSpec2 size_spec() const { return m_layout.value().size_spec; }

    void set_size_spec(Axis axis, SizeSpec spec) {
        m_layout.value().size_spec[(unsigned)axis] = spec;
    }

    Alignment2 alignment() const { return m_layout.value().align; }

    void set_align(Axis axis, Alignment align) {
        m_layout.value().align[(unsigned)axis] = align;
    }

    LayoutSpec const& layout() const { return m_layout.value(); }

protected:
    explicit View(SizeSpec2 size_spec)
        : m_layout(LayoutSpec{size_spec, {Alignment::low, Alignment::low}}) {}

    explicit View(std::reference_wrapper<LayoutSpec> borrored_layout)
        : m_layout(borrored_layout) {}

private:
    virtual void do_display(Rect frame) = 0;

    /// \param max_size can be -1 on either axis, in that case the max size is
    /// unknown
    virtual vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const = 0;

    virtual bool do_is_leaf() const { return true; }

    ValueOrRef<LayoutSpec> m_layout;
};

// =============================================================================
// View adapters
// =============================================================================

namespace internal {
struct EnabledAction {
    Owner<View> operator()(Owner<View>) const;
    Property<bool const> is_enabled;
};
} // namespace internal

inline ViewPipeAdapter<internal::EnabledAction>
enabled(Property<bool const> is_enabled) {
    return ViewPipeAdapter{ui::internal::EnabledAction{std::move(is_enabled)}};
}

namespace internal {
struct FrameAction {
    float thickness;
    Owner<View> operator()(Owner<View>) const;
};
} // namespace internal

inline ViewPipeAdapter<internal::FrameAction> framed(float thickness = 1.0f) {
    return ViewPipeAdapter{ui::internal::FrameAction{thickness}};
}

} // namespace mosaic::ui