#pragma once

#include <utl/stack.hpp>

#include "gui/font.h"

namespace mosaic {

class FontManager;
class RunModeController;

struct UIContext {
    utl::stack<FontContext> font_stack;
    FontManager& font_manager;
    RunModeController& run_mode_controller;
};

UIContext& get_current_ui_context();

} // namespace mosaic