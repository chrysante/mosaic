#pragma once

#include <functional>
#include <string_view>
#include <variant>

#include <utl/hash.hpp>

struct ImFont;

namespace mosaic {

struct FontIcon {
    ImFont* font;
    unsigned code_point;
};

using Icon = std::variant<FontIcon>;

struct IconId {
    static IconId const invalid_id;

    unsigned value;

    bool operator==(IconId const&) const = default;
};

constexpr IconId const IconId::invalid_id = {.value = (unsigned)-1};

namespace ui {

///
Icon resolve_icon(IconId icon_id);

IconId lucide_icon_id(std::string_view name);

inline Icon lucide_icon(std::string_view name) {
    return resolve_icon(lucide_icon_id(name));
}

} // namespace ui

} // namespace mosaic

template <> struct std::hash<mosaic::IconId> {
    std::size_t operator()(mosaic::IconId const& icon_id) const {
        return std::hash<unsigned>{}(icon_id.value);
    }
};