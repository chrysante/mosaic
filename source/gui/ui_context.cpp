#include "gui/ui_context.h"

#include <imgui.h>

using namespace mosaic;

UIContext const* mosaic::get_current_ui_context() {
    return static_cast<UIContext const*>(ImGui::GetIO().UserData);
}