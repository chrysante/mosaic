#pragma once

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

    ///
    void set_font_atlas(ImFontAtlas* atlas) { m_atlas = atlas; }

private:
    ImFontAtlas* m_atlas = nullptr;
    utl::hashmap<FontDesc, ImFont*> m_font_map;
};

} // namespace mosaic