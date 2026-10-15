#include <imgui.h>
#include <imgui_internal.h>
#include <vml/vml.hpp>

#include "gui/font.h"
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
            ToolbarDropdownMenu()
                .content([] {
                    ImGui::Button("Hello");
                }),
            ToolbarIconButton("heart"),
            ToolbarSeparator(),
            ToolbarTextItem()
                .text([] { 
                    return ImGui::IsKeyDown(ImGuiKey_Space) ? "Space down" :
                                                              "Space not down";
                })
                .font(FontDesc::ui_default().set_weight(FontWeight::bold)),
            ToolbarIconButton("star"),
            CustomToolbarElement([](vml::float2 size) {
                auto* DL = ImGui::GetWindowDrawList();
                vml::float2 pos = ImGui::GetCursorScreenPos();
                vml::float4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                color.alpha *= 0.15;
                DL->AddRectFilled(pos, pos + size, 
                                  ImGui::ColorConvertFloat4ToU32(color), 20.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, 0.0f});
                ui::with_font({FontSize::medium - 2}, [&] {
                    ImGui::ButtonEx(
                        "Button", 
                        {ImGui::CalcTextSize("Button").x + 8, size.y-8},
                        ImGuiButtonFlags_AlignTextBaseLine);
                });
                ImGui::PopStyleVar(2);
            }),
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