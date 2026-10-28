#pragma once

#include <vml/vml.hpp>

#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

/// Tries to conform to `size_spec`
/// If `available` is >= 0, this is the maximum value this function returns.
/// Otherwise the parameter is ignored and if `size_spec` is `fill`, it defaults
/// to the intrinsic size
inline float resolve_child_size_axis(SizeSpec size_spec, float intrinsic,
                                     float available) {
    auto clamp = [available](float value) {
        return available >= 0 ? std::min(value, available) : value;
    };

    switch (size_spec.mode()) {
    case SizeSpec::Mode::fixed:
        return clamp(size_spec.fixed_size());
    case SizeSpec::Mode::fit:
        return clamp(intrinsic);
    case SizeSpec::Mode::fill:
        return clamp(available >= 0 ? available : intrinsic);
    }
}

inline vml::float2 resolve_child_size(SizeSpec2 size_spec,
                                      vml::float2 intrinsic,
                                      vml::float2 available) {
    return vml::map(size_spec, intrinsic, available, resolve_child_size_axis);
}

inline float resolve_child_pos_axis(float pos, float size_avail,
                                    float child_size, Alignment align) {
    switch (align) {
    case Alignment::low:
        return pos;
    case Alignment::mid:
        return pos + 0.5f * (size_avail - child_size);
    case Alignment::high:
        return pos + size_avail - child_size;
    }
}

inline vml::float2 resolve_child_pos(vml::float2 min_pos,
                                     vml::float2 size_avail,
                                     vml::float2 child_size, Alignment2 align) {
    return vml::map(min_pos, size_avail, child_size, align,
                    resolve_child_pos_axis);
}

inline Rect resolve_child_frame(vml::float2 min_pos, vml::float2 size_avail,
                                vml::float2 child_size_intrin,
                                LayoutSpec child_layout) {
    vml::float2 size = resolve_child_size(child_layout.size_spec,
                                          child_size_intrin, size_avail);
    size_avail = vml::max(size_avail, size);
    vml::float2 pos =
        resolve_child_pos(min_pos, size_avail, size, child_layout.align);
    return {pos, size};
}

Rect resolve_child_frame(vml::float2 min_pos, vml::float2 size_avail,
                         View const& child);

} // namespace mosaic::ui