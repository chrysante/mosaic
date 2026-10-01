#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <utl/hashtable.hpp>

#include <mosaic/gui/font.h>
#include <mosaic/support/export.h>

namespace mosaic {

///
class MOSAIC_EXPORT FontManager {
public:
    explicit FontManager(float scale_factor = 1.0);

    /// Tries to return the font described by \p desc
    ImFont* get_font(FontDesc const& desc);

    /// \overload for convenience
    ImFont* get_font(FontParameterType auto... params) {
        return get_font(FontDesc::ui_default(params...));
    }

    /// Tries to return the icon described by \p desc of the name \p name
    IconData get_icon(IconFontDesc const& font, std::string_view name);

    /// \overload for convenience
    IconData get_icon(IconSize size, std::string_view name) {
        return get_icon(IconFontDesc{size}, name);
    }

    ///
    void set_font_atlas(ImFontAtlas* a) { atlas = a; }

    ///
    void set_scale_factor(float scale) { scale_factor = scale; }

    ///
    utl::hashmap<std::string, uint16_t> const& get_all_icon_codes() const {
        return all_icon_codes;
    }

private:
    ImFontAtlas* atlas = nullptr;
    utl::hashmap<FontDesc, ImFont*> font_map;
    utl::hashmap<IconFontDesc, ImFont*> icon_font_map;
    /// Maps CSS icon names to unicode representation. This lists all icons
    /// available in the resource files
    utl::hashmap<std::string, uint16_t> all_icon_codes;
    float scale_factor = 1;
};

} // namespace mosaic