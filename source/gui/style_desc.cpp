#include "gui/style_desc.h"

#include <sstream>

#include <nlohmann/json.hpp>

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
    v.base = j.at("base").get<ColorDesc>();
    v.hovered = j.value("hovered", ColorModifier{});
    v.active = j.value("active", ColorModifier{});
}

void mosaic::to_json(nlohmann::json& j, ControlColorDesc const& v) {
    j = nlohmann::json{
        {"base", v.base}, {"hovered", v.hovered}, {"active", v.active}};
}

void mosaic::from_json(nlohmann::json const& j, StyleDesc& v) {
    auto const& colors = j.at("color");
    if (!colors.is_array() || colors.size() != 2)
        throw nlohmann::json::type_error::create(
            302, "color must be an array of exactly 2 elements", &j);
    for (std::size_t i = 0; i < 2; ++i) {
        auto const& c = colors[i];
        v.color[i].control = c.at("control").get<ControlColorDesc>();
        v.color[i].control_accent =
            c.at("control_accent").get<ControlColorDesc>();
        v.color[i].window_background =
            c.at("window_background").get<ColorDesc>();
    }
}

void mosaic::to_json(nlohmann::json& j, StyleDesc const& v) {
    j = nlohmann::json::object();
    j["color"] = nlohmann::json::array();
    for (std::size_t i = 0; i < 2; ++i) {
        auto const& c = v.color[i];
        j["color"].push_back({{"control", c.control},
                              {"control_accent", c.control_accent},
                              {"window_background", c.window_background}});
    }
}
