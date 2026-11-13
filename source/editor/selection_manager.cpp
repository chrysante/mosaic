#include "editor/selection_manager.h"

#include <imgui.h>

using namespace mosaic;

SelectionMode mosaic::get_selection_mode() {
    if (ImGui::IsKeyDown(ImGuiMod_Super)) return SelectionMode::toggle;
    if (ImGui::IsKeyDown(ImGuiMod_Shift)) return SelectionMode::add;
    return SelectionMode::replace;
}
