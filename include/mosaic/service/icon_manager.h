#pragma once

#include <span>
#include <vector>

#include <utl/hashtable.hpp>

#include <mosaic/gui/icon.h>

namespace mosaic {

class IconManager {
public:
    Icon get_icon(IconId icon_id);

    std::vector<IconId>
    add_font_icon_set(ImFont* font, std::span<unsigned const> code_points);

    void set_fallback_icon(Icon fallback_icon) {
        m_fallback_icon = fallback_icon;
    }

private:
    Icon m_fallback_icon;
    unsigned m_icon_id_counter = 0;
    utl::hashmap<IconId, Icon> m_icons;
};

void load_default_icons(IconManager& icon_manager);

} // namespace mosaic