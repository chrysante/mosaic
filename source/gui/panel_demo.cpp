#if 0

#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <vml/vml.hpp>

#include "gui/panel.h"
#include "gui/panel_registry.h"
#include "gui/toolbar.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "support/json_support.h"

using namespace mosaic;

namespace {

struct DemoPanel : Panel {
    DemoPanel();

    void display() override;

    nlohmann::json save_settings() const override;
    void load_settings(nlohmann::json const&) override;

    Owner<ui::View> make_toolbar() const;

    vml::int2 padding = {-1, -1};
    std::string config_string;
};

} // namespace

MOSAIC_REGISTER_VIEW(DemoPanel, {.name = "MosaicDemoPanel"})

DemoPanel::DemoPanel() { set_toolbar(make_toolbar()); }

void DemoPanel::display() {
    ImGui::Button("Test Button");
    ImGui::Text("Title: %s (%s)", title().c_str(), type_name().c_str());
    if (ImGui::Button("Close Panel")) request_close();
    bool have_toolbar = toolbar() != nullptr;
    if (ImGui::Checkbox("Toolbar", &have_toolbar)) {
        if (have_toolbar) set_toolbar(make_toolbar());
        else set_toolbar(nullptr);
    }
    if (ImGui::SliderInt2("Window Padding", padding.data(), -1, 20))
        set_padding(padding);
    ImGui::InputTextMultiline("Config Text", &config_string);
}

namespace config_key { static constexpr std::string_view string = "string"; }

nlohmann::json DemoPanel::save_settings() const {
    auto object = nlohmann::json::object();
    object[config_key::string] = config_string;
    return object;
}

void DemoPanel::load_settings(nlohmann::json const& object) {
    config_string = json_find<std::string>(object, config_key::string)
                        .value_or("Empty config string");
}

namespace {

struct CustomToolbarElement : ui::View {};

} // namespace

Owner<ui::View> DemoPanel::make_toolbar() const {
    // clang-format off
    return ui::toolbar({
#if 0
        ToolbarDropdownMenu()
            .content([] {
                ImGui::Button("Hello");
            }),
#endif
        ui::button(ui::lucide_icon("heart")),
        ui::vseparator(),
        ui::label(ui::computed([] -> std::string { 
            return ImGui::IsKeyDown(ImGuiKey_Space) ? "Space down" : 
                                                      "Space not down";
        })),
        ui::button(ui::lucide_icon("star")),
#if 0
        CustomToolbarElement([](vml::float2 size) {
            auto* DL = ImGui::GetWindowDrawList();
            vml::float2 pos = ImGui::GetCursorScreenPos();
            vml::float4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            color.alpha *= 0.15;
            DL->AddRectFilled(pos, pos + size, 
                                ImGui::ColorConvertFloat4ToU32(color), 20.0f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, 0.0f});
            ImGui::ButtonEx("Button", 
                            {ImGui::CalcTextSize("Button").x + 8, size.y-8},
                            ImGuiButtonFlags_AlignTextBaseLine);
            ImGui::PopStyleVar(2);
        })
            .width(100)
            .use_width_avail(),
#endif
        ui::spacer(),
        ui::button("Some Button")
    });
    // clang-format on
}

#endif