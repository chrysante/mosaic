#pragma once

#include <cstdint>

#include <vml/vml.hpp>

namespace mosaic {

struct StyleDesc;

// Concrete style colors and sizes

struct Color { std::uint32_t value; };

struct ControlColor {
    Color base;
    Color hovered;
    Color active;
};

enum struct StyleType { light, dark };

struct Style_ {
    StyleType style_type;

    struct {
        ControlColor control;
        ControlColor control_accent;

        Color window_background;
    } color;
};

Style_ compute_from_desc(StyleDesc const& style_desc, StyleType style_type);

void apply_to_imgui(Style_ const& style);

Style_ const& get_global_style2();
Style_& get_global_style_write();

} // namespace mosaic