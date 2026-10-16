#include "gui/font.h"

#include "gui/ui_context.h"
#include "service/font_manager.h"

using namespace mosaic;

FontDesc FontDesc::ui_default() {
    FontDesc font{};
    font.weight = FontWeight::regular;
    font.style = FontStyle::roman;
    return font;
}

FontDesc FontDesc::mono_default() {
    FontDesc font{};
    font.weight = FontWeight::regular;
    font.style = FontStyle::monospaced;
    return font;
}

ImFont* ui::font(FontDesc const& desc) {
    auto* ui_context = get_current_ui_context();
    if (!ui_context) return nullptr;
    return ui_context->font_manager.get_font(desc);
}

IconData ui::icon_data(std::string_view name) {
    auto* ui_context = get_current_ui_context();
    if (!ui_context) return {};
    return ui_context->font_manager.get_icon(name);
}