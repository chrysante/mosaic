#include <imgui.h>
#include <vml/vml.hpp>

#include "gui/toolbar.h"
#include "gui/view.h"
#include "gui/view_registry.h"

using namespace mosaic;

namespace {

struct DemoView : View {
    DemoView();

    void display() override;

    Toolbar make_toolbar() const {
        // clang-format off
        return {
            ToolbarIconButton("link"),
            ToolbarSpacer(), 
            ToolbarButton("Some Button")
        };
        // clang-format on
    }

    vml::int2 padding = {-1, -1};
};

} // namespace

MOSAIC_REGISTER_VIEW(DemoView, "MosaicDemoView")

DemoView::DemoView() { set_toolbar(make_toolbar()); }

void DemoView::display() {
    ImGui::Button("Test Button");
    ImGui::Text("Title: %s (%s)", title().c_str(), type_name().c_str());
    if (ImGui::Button("Close View")) request_close();
    bool have_toolbar = uses_toolbar();
    if (ImGui::Checkbox("Toolbar", &have_toolbar)) {
        if (have_toolbar) set_toolbar(make_toolbar());
        else remove_toolbar();
    }
    if (ImGui::SliderInt2("View Padding", padding.data(), -1, 20))
        set_padding(padding);
}