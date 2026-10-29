#pragma once

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>
#include <mosaic/support/memory.h>

namespace mosaic::ui {

Owner<View> toolbar(ViewContainer<> elements);

template <std::size_t N> Owner<View> toolbar(Owner<View> (&&elements)[N]) {
    return toolbar(ViewContainer<>(std::move(elements)));
}

namespace internal {

struct ToolbarPadding2 {
    float top;
    float bottom;
    float left;
    float right;
};

void set_toolbar_padding(View* toolbar, ToolbarPadding2 padding);

} // namespace internal

} // namespace mosaic::ui
