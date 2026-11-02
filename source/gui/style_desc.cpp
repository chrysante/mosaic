#include "gui/style_desc.h"

#include <sstream>

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include "support/json_support.h"

using namespace mosaic;

void mosaic::from_json(nlohmann::json const& j, ColorModifier& v) {
    v.saturation_scale = j.value("saturation_scale", 1.0f);
    v.value_scale = j.value("value_scale", 1.0f);
    v.alpha_scale = j.value("alpha_scale", 1.0f);
}

void mosaic::to_json(nlohmann::json& j, ColorModifier const& v) {
    j = nlohmann::json{{"saturation_scale", v.saturation_scale},
                       {"value_scale", v.value_scale},
                       {"alpha_scale", v.alpha_scale}};
}

void mosaic::from_json(nlohmann::json const& j, ColorDesc& v) {
    auto const& s = j.get<std::string>();
    if (s.size() != 9 || s[0] != '#')
        throw nlohmann::json::type_error::create(
            302, "color must be an 8-character hex string", &j);
    v.value = static_cast<std::uint32_t>(std::stoul(s.substr(1), nullptr, 16));
}

void mosaic::to_json(nlohmann::json& j, ColorDesc const& v) {
    std::ostringstream ss;
    ss << '#' << std::hex << std::setfill('0') << std::setw(8) << v.value;
    j = ss.str();
}

void mosaic::from_json(nlohmann::json const& j, ControlColorDesc& v) {
    v.background = json_find(j, "background", ColorDesc{});
    v.background_hovered = json_find(j, "background_hovered", ColorDesc{});
    v.background_active = json_find(j, "background_active", ColorDesc{});
    v.border = json_find(j, "border", ColorDesc{});
    v.text = json_find(j, "text", ColorDesc{});
}

void mosaic::to_json(nlohmann::json& j, ControlColorDesc const& v) {
    j = nlohmann::json{{
                           "background",
                           v.background,
                       },
                       {"background_hovered", v.background_hovered},
                       {"background_active", v.background_active},
                       {"border", v.border},
                       {"text", v.text}};
}

void mosaic::from_json(nlohmann::json const& j, StyleDesc& v) {
    auto const& colors = j.at("color");
    if (!colors.is_array() || colors.size() != 2)
        throw nlohmann::json::type_error::create(
            302, "color must be an array of exactly 2 elements", &j);
    for (std::size_t i = 0; i < 2; ++i) {
        auto const& c = colors[i];
        for (std::size_t j = 0; j < enum_count<StyleColor>; ++j) {
            StyleColor index = (StyleColor)j;
            std::string name(magic_enum::enum_name(index));
            v.color[i][index] = json_find(c, name, ColorDesc{});
        }
        for (std::size_t j = 0; j < enum_count<StyleColorControl>; ++j) {
            StyleColorControl index = (StyleColorControl)j;
            std::string name(magic_enum::enum_name(index));
            v.control_color[i][index] = json_find(c, name, ControlColorDesc{});
        }
    }
}

void mosaic::to_json(nlohmann::json& j, StyleDesc const& v) {
    auto color_array = nlohmann::json::array();
    for (std::size_t i = 0; i < 2; ++i) {
        auto colors = nlohmann::json::object();
        for (std::size_t j = 0; j < enum_count<StyleColor>; ++j) {
            StyleColor index = (StyleColor)j;
            std::string name(magic_enum::enum_name(index));
            colors[name] = v.color[i][index];
        }
        for (std::size_t j = 0; j < enum_count<StyleColorControl>; ++j) {
            StyleColorControl index = (StyleColorControl)j;
            std::string name(magic_enum::enum_name(index));
            colors[name] = v.control_color[i][index];
        }
        color_array.push_back(std::move(colors));
    }
    j = nlohmann::json::object();
    j["color"] = std::move(color_array);
}
