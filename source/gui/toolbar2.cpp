#include "gui/toolbar2.h"
#include "gui/containers.h"
#include "gui/view_support.h"
#include "vml/vector.hpp"

using namespace mosaic;
using namespace ui;

namespace {

struct ToolbarView : View {
    ToolbarView(ViewContainer<>&& elements)
        : View({SizeSpec::fill(), SizeSpec::fill()})
        , stack(hstack(std::move(elements), Alignment::mid)) {}

    void do_display(Rect frame) override {
        frame.size.x -= padding.left + padding.right;
        frame.size.y -= padding.top + padding.bottom;
        frame.origin.x += padding.left;
        frame.origin.y += padding.top;
        stack->display(frame);
    }

    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override {
        vml::float2 total_padding = {padding.left + padding.right,
                                     padding.top + padding.bottom};
        vml::float2 stack_max_size =
            vml::map(max_size, total_padding, [](float size, float padding) {
                if (size < 0.0f) return size;
                return std::max(0.0f, size - padding);
            });
        return stack->calc_intrinsic_size(stack_max_size) + total_padding;
    }

    bool do_is_leaf() const override { return false; }

    Owner<StackView> stack;
    ui::internal::ToolbarPadding2 padding;
};

} // namespace

Owner<View> ui::toolbar(ViewContainer<> elements) {
    return allocate<ToolbarView>(std::move(elements));
}

void ui::internal::set_toolbar_padding(View* view, ToolbarPadding2 padding) {
    auto* toolbar = dynamic_cast<ToolbarView*>(view);
    if (toolbar) toolbar->padding = padding;
}