#include "gui/view.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "application/debug_settings.h"

using namespace mosaic;
using namespace ui;

#if !defined(NDEBUG)
void View::display(Rect frame) {
    do_display(frame);
    if (want_debug_overlays()) {
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddRect(frame.min(), frame.max(), IM_COL32(255, 128, 0, 200));
    }
}
#endif