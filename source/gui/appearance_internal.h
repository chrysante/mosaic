#pragma once

#include "gui/appearance.h"

namespace mosaic {

void update_style(bool force = false);

AppearanceType get_system_appearance();

Style make_style(AppearanceType type);

} // namespace mosaic