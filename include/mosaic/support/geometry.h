#pragma once

#include <nlohmann/json_fwd.hpp>
#include <vml/vml.hpp>

namespace mosaic {

struct Rect {
    // The top-left corner of the rectangle (by convention)
    vml::float2 min() const { return origin; }

    // The bottom-right corner of the rectangle (by convention)
    vml::float2 max() const { return origin + size; }

    vml::float2 origin;
    vml::float2 size;
};

void to_json(nlohmann::json&, Rect const&);
void from_json(nlohmann::json const&, Rect&);

} // namespace mosaic