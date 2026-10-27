#include "gui/view.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "application/debug_settings.h"
#include "gui/view_debugger.h"

using namespace mosaic;
using namespace ui;

void View::display(Rect frame) {
    assert(vml::isfinite(frame.origin).all());
    assert(vml::isfinite(frame.size).all());
#ifndef NDEBUG
    debug_begin_node(*this);
#endif
    bool const disabled = !enabled();
    if (disabled) ImGui::BeginDisabled();
    do_display(frame);
    if (disabled) ImGui::EndDisabled();
#ifndef NDEBUG
    if (want_debug_overlays()) {
        auto* DL = ImGui::GetWindowDrawList();
        DL->AddRect(frame.min(), frame.max(), IM_COL32(64, 255, 32, 200));
    }
    debug_end_node();
#endif
}

vml::float2 View::calc_intrinsic_size(vml::float2 max_size) const {
    assert((vml::isfinite(max_size) | vml::isinf(max_size)).all());
    return do_calc_intrinsic_size(max_size);
}