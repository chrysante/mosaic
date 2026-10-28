#include "gui/property_editor.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "gui/containers.h"
#include "gui/view.h"
#include "gui/view_layout_support.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "support/memory.h"

using namespace mosaic;
using namespace ui;

namespace {

struct ControlWrapper : View {
    explicit ControlWrapper(float weight, Owner<View> control)
        : View({SizeSpec::fill(weight), SizeSpec::fit()})
        , control(std::move(control)) {}

    void do_display(Rect frame) {
        Rect control_frame =
            resolve_child_frame(frame.origin, frame.size, *control);
        control->display(control_frame);
    }

    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const {
        return control->calc_intrinsic_size(max_size);
    }

    bool do_is_leaf() const { return false; }

    Owner<View> control;
};

} // namespace

Owner<View> ui::property(Property<std::string const> label,
                         Owner<View> control) {
    auto label_view = ui::label(std::move(label)) | hfill_weight(3.0f);
    auto control_view = allocate<ControlWrapper>(7.0f, std::move(control));
    return hstack({std::move(label_view), std::move(control_view)}) | hfill;
}

namespace {
struct SectionView : View {
    explicit SectionView(ViewContainer<> children)
        : View({SizeSpec::fill(), SizeSpec::fit()}) {
        std::size_t num_gaps = children.empty() ? 0 : children.size() - 1;
        ViewContainer<> stack_items;
        stack_items.reserve(children.size() + num_gaps);
        auto stolen_children = children.steal_elements();
        int i = 0;
        for (auto& child : stolen_children) {
            if (i++ > 0) stack_items.push_back(hseparator());
            stack_items.push_back(std::move(child));
        }
        stack = ui::vstack(std::move(stack_items));
    }

    void do_display(Rect frame) { stack->display(frame); }

    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const {
        return stack->calc_intrinsic_size(max_size);
    }

    bool do_is_leaf() const { return false; }

    Owner<StackView> stack;
};

} // namespace

Owner<View> ui::section(ViewContainer<> children) {
    return allocate<SectionView>(std::move(children));
}
