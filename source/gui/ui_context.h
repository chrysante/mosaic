#pragma once

#include <string>

#include <utl/hashtable.hpp>
#include <utl/stack.hpp>

#include "gui/font.h"
#include "gui/icon.h"

namespace mosaic {

class FontManager;
class IconManager;
class RunModeController;

struct UIContext {
    utl::stack<FontContext> font_stack;
    FontManager& font_manager;
    utl::hashmap<std::string, IconId> lucide_icon_table;
    IconManager& icon_manager;
    RunModeController& run_mode_controller;
};

UIContext& get_current_ui_context();

} // namespace mosaic