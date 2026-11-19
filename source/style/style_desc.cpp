#include "style/style_desc.h"

#include <exception>
#include <span>
#include <sstream>

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include "nlohmann/adl_serializer.hpp"
#include "style/style_class.h"
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

StyleDesc::StyleDesc() {
    auto& style_class = StyleClass::get();
    colors.resize(style_class.num_style_vars(StyleVarType::color));
    control_colors.resize(
        style_class.num_style_vars(StyleVarType::control_color));
}

void mosaic::from_json(nlohmann::json const& j, StyleDesc& v) {
    auto& style_class = StyleClass::get();
    if (!j.is_object())
        throw nlohmann::json::type_error::create(503, "Invalid style var set",
                                                 &j);
    for (auto& [group_name, group_value] : j.items()) {
        auto* group = style_class.find_group(group_name);
        if (!group) {
            Logger::error(
                "Failed to deserialize style: Unknown style var group named {}",
                group_name);
            continue;
        }
        for (auto& [name, value] : group_value.items()) {
            auto var_key = group->find_style_var(name);
            if (!var_key) {
                Logger::error(
                    "Failed to deserialize style var \"{}\": does not exist",
                    name);
                continue;
            }
            switch (var_key->type) {
            case StyleVarType::color:
                if (!value.is_array() || value.size() != 2) {
                    Logger::error(
                        "Failed to deserialize style var \"{}\": must be array with 2 elements",
                        name);
                    continue;
                }
                if (auto color = json_try_get<ColorDesc>(value[0]))
                    v.colors[var_key->index][0] = *color;
                if (auto color = json_try_get<ColorDesc>(value[1]))
                    v.colors[var_key->index][1] = *color;
                break;
            case StyleVarType::control_color:
                if (!value.is_array() || value.size() != 2) {
                    Logger::error(
                        "Failed to deserialize style var \"{}\": must be array with 2 elements",
                        name);
                    continue;
                }
                if (auto color = json_try_get<ControlColorDesc>(value[0]))
                    v.control_colors[var_key->index][0] = *color;
                if (auto color = json_try_get<ControlColorDesc>(value[1]))
                    v.control_colors[var_key->index][1] = *color;
                break;
            case StyleVarType::size:
            case StyleVarType::size2:
                assert(false);
                std::terminate();
            }
        }
    }
}

template <typename T, std::size_t N>
struct nlohmann::adl_serializer<std::array<T, N>> {
    static void to_json(nlohmann::json& j, std::array<T, N> const& v) {
        j = nlohmann::json::array();
        for (std::size_t i = 0; i < v.size(); ++i)
            j[i] = v[i];
    }
};

template <StyleVarType Type, typename T>
static void write_style_var_set(nlohmann::json& j,
                                std::span<std::array<T, 2> const> set) {
    auto& style_class = StyleClass::get();
    for (unsigned i = 0; i < set.size(); ++i) {
        auto const& md = style_class.get_metadata(StyleVarId<Type>{i});
        j[md.group][md.name] = set[i];
    }
}

void mosaic::to_json(nlohmann::json& j, StyleDesc const& v) {
    j = nlohmann::json::object();
    write_style_var_set<StyleVarType::color, ColorDesc>(j, v.colors);
    write_style_var_set<StyleVarType::control_color, ControlColorDesc>(
        j, v.control_colors);
}
