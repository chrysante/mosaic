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

    bool enabled() const { return m_enabled.materialize(); }

    void set_enabled(Property<bool const> enabled) {
        m_enabled = std::move(enabled);
    }

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
    Property<bool const> m_enabled = true;
};

// =============================================================================
// Pipes
// =============================================================================

constexpr auto fit(Axis axis) {
    return ViewPipeAdapter{[axis]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
        if (view) view->set_size_spec(axis, SizeSpec::fit());
        return std::forward<ViewPtr>(view);
    }};
}

inline constexpr auto hfit = fit(Axis::horizontal);

inline constexpr auto vfit = fit(Axis::vertical);

constexpr auto fill_weight(Axis axis, float weight) {
    return ViewPipeAdapter{
        [axis, weight]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_size_spec(axis, SizeSpec::fill(weight));
            return std::forward<ViewPtr>(view);
        }};
}

constexpr auto fill(Axis axis) { return fill_weight(axis, 1.0f); }

inline constexpr auto hfill = fill(Axis::horizontal);

inline constexpr auto vfill = fill(Axis::vertical);

constexpr inline auto hfill_weight(float weight) {
    return fill_weight(Axis::horizontal, weight);
}

constexpr inline auto vfill_weight(float weight) {
    return fill_weight(Axis::horizontal, weight);
}

constexpr inline auto fixed_size(Axis axis, float size) {
    return ViewPipeAdapter{
        [axis, size]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_size_spec(axis, SizeSpec::fixed(size));
            return std::forward<ViewPtr>(view);
        }};
}

constexpr inline auto fixed_width(float size) {
    return fixed_size(Axis::horizontal, size);
}

constexpr inline auto fixed_height(float size) {
    return fixed_size(Axis::vertical, size);
}

constexpr auto align(Axis axis, Alignment alignment) {
    return ViewPipeAdapter{
        [axis, alignment]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_align(axis, alignment);
            return std::forward<ViewPtr>(view);
        }};
}

constexpr auto align_left = align(Axis::horizontal, Alignment::low);

constexpr auto align_right = align(Axis::horizontal, Alignment::high);

constexpr auto align_top = align(Axis::vertical, Alignment::low);

constexpr auto align_bottom = align(Axis::vertical, Alignment::high);

constexpr auto hcenter = align(Axis::horizontal, Alignment::mid);

constexpr auto vcenter = align(Axis::vertical, Alignment::mid);

constexpr auto center = hcenter | vcenter;

inline auto enabled(Property<bool const> is_enabled) {
    return ViewPipeAdapter{
        [is_enabled =
             std::move(is_enabled)]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_enabled(is_enabled);
            return std::forward<ViewPtr>(view);
        }};
}

namespace internal {
struct FrameAction { Owner<View> operator()(Owner<View>) const; };
} // namespace internal

inline ViewPipeAdapter<internal::FrameAction> framed() {
    return ViewPipeAdapter{ui::internal::FrameAction{}};
}

} // namespace mosaic::ui