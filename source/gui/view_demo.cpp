#include <imgui.h>

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
};

} // namespace

MOSAIC_REGISTER_VIEW(DemoView, "MosaicDemoView")

DemoView::DemoView() { set_toolbar(make_toolbar()); }

void DemoView::display() {
    ImGui::Text("Title: %s (%s)", title().c_str(), type_name().c_str());
    if (ImGui::Button("Close View")) request_close();
    bool have_toolbar = toolbar() != nullptr;
    if (ImGui::Checkbox("Toolbar", &have_toolbar)) {
        if (have_toolbar) set_toolbar(make_toolbar());
        else remove_toolbar();
    }
}