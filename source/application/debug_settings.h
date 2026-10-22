#pragma once

#include "gui/command_palette.h"
namespace mosaic {

#ifndef NDEBUG

/// Global flag to indicate whether debug overlays should be displayed or not
bool want_debug_overlays();

class CommandPalette;

// The applications command palette.
// This exists to make it easy for debug code to temporarily add commands, but
// is only available in debug mode. All other code must obtain a command
// palette by other means to register commands.
void set_debug_command_palette(CommandPalette&);
CommandPalette& get_debug_command_palette();

#endif // !defined(NDEBUG)

} // namespace mosaic