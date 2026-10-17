#pragma once

namespace mosaic {

class FontManager;
class RunModeController;

struct UIContext {
    FontManager& font_manager;
    RunModeController& run_mode_controller;
};

UIContext const* get_current_ui_context();

} // namespace mosaic