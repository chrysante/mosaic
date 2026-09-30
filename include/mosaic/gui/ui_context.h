#pragma once

namespace mosaic {

class FontManager;

struct UIContext {
    FontManager& font_manager;
};

UIContext const* get_current_ui_context();

} // namespace mosaic