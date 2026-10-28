#include "gui/view_support.h"
#include "gui/view_layout_support.h"

#include "gui/view.h"

using namespace mosaic;
using namespace ui;

Rect ui::resolve_child_frame(vml::float2 min_pos, vml::float2 size_avail,
                             View const& child) {
    // FIXME: We don't need to make the expensive call to
    // child.calc_intrinsic_size() in all cases
    vml::float2 child_size_intrin = child.calc_intrinsic_size(size_avail);
    return resolve_child_frame(min_pos, size_avail, child_size_intrin,
                               child.layout());
}