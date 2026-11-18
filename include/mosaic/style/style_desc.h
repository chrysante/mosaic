#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/style/style_class.h>
#include <mosaic/support/enum.h>

namespace mosaic {

struct ColorModifier {
    float saturation_scale = 1.0f;
    float value_scale = 1.0f;
    float alpha_scale = 1.0f;
};

void from_json(nlohmann::json const&, ColorModifier&);
void to_json(nlohmann::json&, ColorModifier const&);

struct ColorDesc {
    // For now, later this may become a variant that supports deriving from
    // system colors etc.
    std::uint32_t value;
};

void from_json(nlohmann::json const&, ColorDesc&);
void to_json(nlohmann::json&, ColorDesc const&);

struct ControlColorDesc {
    ColorDesc background;
    ColorDesc background_hovered;
    ColorDesc background_active;
    ColorDesc border;
    ColorDesc text;
};

void from_json(nlohmann::json const&, ControlColorDesc&);
void to_json(nlohmann::json&, ControlColorDesc const&);

// Style description that is user editable and stored to disk
struct StyleDesc {
    StyleDesc();

    std::vector<std::array<ColorDesc, 2>> colors;
    std::vector<std::array<ControlColorDesc, 2>> control_colors;
};

void from_json(nlohmann::json const&, StyleDesc&);
void to_json(nlohmann::json&, StyleDesc const&);

} // namespace mosaic