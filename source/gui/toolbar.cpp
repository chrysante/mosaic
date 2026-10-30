#include "gui/toolbar.h"

#include <imgui.h>
#include <vml/vml.hpp>

#include "gui/containers.h"
#include "gui/view_support.h"

using namespace mosaic;
using namespace ui;

namespace {

struct ToolbarView : View {
    ToolbarView(ViewContainer<>&& elements)
        : View({SizeSpec::fill(), SizeSpec::fill()})
        , stack(hstack(std::move(elements), Alignment::mid)) {}

    void do_display(Rect frame) override {
        ImGui::PushStyleColor(ImGuiCol_Border, 0);
        ImGui::PushStyleColor(ImGuiCol_BorderShadow, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              IM_COL32(255, 255, 255, 32));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              IM_COL32(255, 255, 255, 48));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 12.0f);

        frame.size.x -= padding.left + padding.right;
        frame.size.y -= padding.top + padding.bottom;
        frame.origin.x += padding.left;
        frame.origin.y += padding.top;
        stack->display(frame);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
    }

    vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const override {
        vml::float2 total_padding = {padding.left + padding.right,
                                     padding.top + padding.bottom};
        vml::float2 stack_max_size =
            vml::map(max_size, total_padding, [](float size, float padding) {
                if (size < 0.0f) return size;
                return std::max(0.0f, size - padding);
            });
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 12.0f);
        vml::float2 result =
            stack->calc_intrinsic_size(stack_max_size) + total_padding;
        ImGui::PopStyleVar();
        return result;
    }

    bool do_is_leaf() const override { return false; }

    Owner<StackView> stack;
    ui::internal::ToolbarPadding padding;
};

} // namespace

Owner<View> ui::toolbar(ViewContainer<> elements) {
    auto views = elements.steal_elements();
    for (auto& view : views)
        view |= vcenter | vfill;
    return allocate<ToolbarView>(std::move(views));
}

void ui::internal::set_toolbar_padding(View* view, ToolbarPadding padding) {
    auto* toolbar = dynamic_cast<ToolbarView*>(view);
    if (toolbar) toolbar->padding = padding;
}