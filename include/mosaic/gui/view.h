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

    SizeSpec2 size_spec() const { return m_size_spec; }

    void set_size_spec(unsigned axis, SizeSpec spec) {
        m_size_spec[axis] = spec;
    }

    Alignment2 alignment_requirement() const { return m_alignment_requirement; }

    void set_alignment_requirement(unsigned axis, Alignment align) {
        m_alignment_requirement[axis] = align;
    }

    bool enabled() const { return m_enabled.materialize(); }

    void set_enabled(Property<bool const> enabled) {
        m_enabled = std::move(enabled);
    }

protected:
    View(SizeSpec2 size_requirement)
        : m_size_spec(size_requirement) {}

private:
    virtual void do_display(Rect frame) = 0;

    /// \param max_size can be -1 on either axis, in that case the max size is
    /// unknown
    virtual vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const = 0;

    virtual bool do_is_leaf() const { return true; }

    SizeSpec2 m_size_spec;
    Alignment2 m_alignment_requirement{Alignment::low, Alignment::low};
    Property<bool const> m_enabled = true;
};

// =============================================================================
// Pipes
// =============================================================================

constexpr auto fit(unsigned axis) {
    return ViewPipeAdapter{
        [axis](View& view) { view.set_size_spec(axis, SizeSpec::fit()); }};
}

inline constexpr auto hfit = fit(0);

inline constexpr auto vfit = fit(1);

constexpr auto fill_weight(unsigned axis, float weight) {
    return ViewPipeAdapter{[axis, weight](View& view) {
        view.set_size_spec(axis, SizeSpec::fill(weight));
    }};
}

constexpr auto fill(unsigned axis) { return fill_weight(axis, 1.0f); }

inline constexpr auto hfill = fill(0);

inline constexpr auto vfill = fill(1);

constexpr inline auto hfill_weight(float weight) {
    return fill_weight(0, weight);
}

constexpr inline auto vfill_weight(float weight) {
    return fill_weight(1, weight);
}

constexpr inline auto fixed_size(unsigned axis, float size) {
    return ViewPipeAdapter{[axis, size](View& view) {
        view.set_size_spec(axis, SizeSpec::fixed(size));
    }};
}

constexpr inline auto fixed_width(float size) { return fixed_size(0, size); }

constexpr inline auto fixed_height(float size) { return fixed_size(1, size); }

constexpr auto align(unsigned axis, Alignment alignment) {
    return ViewPipeAdapter{[axis, alignment](View& view) {
        view.set_alignment_requirement(axis, alignment);
    }};
}

constexpr auto align_left = align(0, Alignment::low);

constexpr auto align_right = align(0, Alignment::high);

constexpr auto align_top = align(1, Alignment::low);

constexpr auto align_bottom = align(1, Alignment::high);

constexpr auto hcenter = align(0, Alignment::mid);

constexpr auto vcenter = align(1, Alignment::mid);

constexpr auto center = hcenter | vcenter;

inline auto enabled(Property<bool const> is_enabled) {
    return ViewPipeAdapter{[is_enabled = std::move(is_enabled)](View& view) {
        view.set_enabled(is_enabled);
    }};
}

} // namespace mosaic::ui