#include "gui/font.h"

#include "application/font_manager.h"
#include "gui/ui_context.h"

using namespace mosaic;

FontDesc FontDesc::ui_default() {
    FontDesc font{};
    font.size = FontSize::medium;
    font.weight = FontWeight::regular;
    font.style = FontStyle::roman;
    return font;
}

FontDesc FontDesc::mono_default() {
    FontDesc font{};
    font.size = FontSize{14};
    font.weight = FontWeight::regular;
    font.style = FontStyle::monospaced;
    return font;
}

ImFont* ui::font(FontDesc const& desc) {
    auto* ui_context = get_current_ui_context();
    if (!ui_context)
        return nullptr;
    return ui_context->font_manager.get_font(desc);
}