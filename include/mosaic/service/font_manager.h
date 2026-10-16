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
    explicit FontManager();

    /// Tries to return the font described by \p desc
    ImFont* get_font(FontDesc const& desc);

    /// \overload for convenience
    ImFont* get_font(FontParameterType auto... params) {
        return get_font(FontDesc::ui_default(params...));
    }

    /// Tries to return the icon described by \p desc of the name \p name
    IconData get_icon(std::string_view name);

    ///
    void set_font_atlas(ImFontAtlas* atlas) { m_atlas = atlas; }

    ///
    utl::hashmap<std::string, uint16_t> const& get_all_icon_codes() const {
        return m_all_icon_codes;
    }

private:
    ImFontAtlas* m_atlas = nullptr;
    utl::hashmap<FontDesc, ImFont*> m_font_map;
    ImFont* m_icon_font = nullptr;
    /// Maps CSS icon names to unicode representation. This lists all icons
    /// available in the resource files
    utl::hashmap<std::string, uint16_t> m_all_icon_codes;
};

} // namespace mosaic