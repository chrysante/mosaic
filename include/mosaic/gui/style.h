#pragma once

#include <vml/vml.hpp>

namespace mosaic {

struct Style2 {
    struct Colors {
#define MOSAIC_STYLE_COLOR(name, category) vml::float4 name;
#include <mosaic/gui/style.def>
    };

    Colors colors;
};

} // namespace mosaic