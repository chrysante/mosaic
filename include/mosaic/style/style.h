#pragma once

#include <cstdint>
#include <vector>

#include <utl/stack.hpp>
#include <vml/vml.hpp>

#include <mosaic/style/style_class.h>
#include <mosaic/support/enum.h>

namespace mosaic {

struct Color { std::uint32_t value; };

struct ControlColor {
    Color background;
    Color background_hovered;
    Color background_active;
    Color border;
    Color text;
};

enum class StyleType { light, dark };

class Style {
public:
    Style();

    Color get_color(StyleColorId id) const { return m_colors[id.index]; }

    ControlColor get_color(StyleControlColorId id) const {
        return m_control_colors[id.index];
    }

private:
    friend class StyleContext;

    std::vector<Color> m_colors;
    std::vector<ControlColor> m_control_colors;
};

Style const& get_global_style();

void push_style_color(StyleColorId index, Color value);
void pop_style_color();
void push_style_control_color(StyleControlColorId index, ControlColor value);
void pop_style_control_color();

namespace style_color {

extern StyleColorId const text;
extern StyleColorId const text_disabled;
extern StyleColorId const text_link;
extern StyleColorId const border;
extern StyleColorId const separator;
extern StyleColorId const scrollbar;
extern StyleColorId const docking_preview;
extern StyleColorId const window_background;

extern StyleControlColorId const control;
extern StyleControlColorId const control_accent;
extern StyleControlColorId const control_toolbar;
extern StyleControlColorId const control_tab;
extern StyleControlColorId const control_tab_inactive;

} // namespace style_color

} // namespace mosaic