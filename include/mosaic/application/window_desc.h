#pragma once

#include <vml/vml.hpp>

namespace mosaic {

///
struct WindowDesc {
    vml::int2 position;
    vml::int2 size;
};

} // namespace mosaic