#include "gui/view.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "application/debug_settings.h"
#include "gui/view_debugger.h"

using namespace mosaic;
using namespace ui;

#ifndef NDEBUG
void View::display(Rect frame) {
    debug_begin_node(*this);
    do_display(frame);
    if (want_debug_overlays()) {
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddRect(frame.min(), frame.max(), IM_COL32(64, 255, 32, 200));
    }
    debug_end_node();
}
#else
void View::display(Rect frame) { do_display(frame); }
#endif
