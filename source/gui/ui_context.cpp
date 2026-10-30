#include "gui/ui_context.h"

#include <imgui.h>

using namespace mosaic;

UIContext& mosaic::get_current_ui_context() {
    return *static_cast<UIContext*>(ImGui::GetIO().UserData);
}