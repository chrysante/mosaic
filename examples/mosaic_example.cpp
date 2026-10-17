#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <mosaic/application/application.h>
#include <mosaic/application/input.h>
#include <mosaic/application/window.h>
#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/dockspace.h>
#include <mosaic/gui/font.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/view_manager.h>
#include <mosaic/gui/view_registry.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/service_context.h>

using namespace mosaic;

static Application* g_app;

static std::string to_string(RunLoopMode mode) {
    switch (mode) {
    case RunLoopMode::Poll:
        return "Poll";
    case RunLoopMode::Wait:
        return "Wait";
    }
}

class ExampleView : public View {
public:
    ExampleView()
        : View() {
        // clang-format off
            set_toolbar({
                ToolbarIconButton("link")
                    .on_click([] {})
            });
        // clang-format on
    }

private:
    void display() override {
        ImGui::Button("My View Button");
        float t = (float)ImGui::GetTime();
        float v = fmodf(t, 1.0f);
        ImGui::ProgressBar(v, ImVec2(100, 0));
        auto* viewport = ImGui::GetWindowViewport();
        ImGui::Text("Position: %f, %f", viewport->Pos.x, viewport->Pos.y);
        ImGui::Text("Size: %f, %f", viewport->Size.x, viewport->Size.y);

        run_loop_mode = g_app->run_loop_mode();
        std::string preview = to_string(run_loop_mode);
        if (ImGui::BeginCombo("Run Loop Mode", preview.c_str())) {
            for (int i = 0; i < 2; ++i) {
                auto mode = (RunLoopMode)i;
                if (ImGui::MenuItem(to_string(mode).c_str(), nullptr,
                                    mode == run_loop_mode))
                    g_app->set_run_loop_mode(mode);
            }
            ImGui::EndCombo();
        }
    }

    RunLoopMode run_loop_mode{};
};

MOSAIC_REGISTER_VIEW(ExampleView, "ExampleView")

class ExampleApp : public Application {
public:
    ExampleApp()
        : event_receiver(service_context().make_event_receiver())
        , event_emitter(service_context().make_event_emitter()) {
        // clang-format off
        command_palette().add({
            .name = "My Command",
            .key_comb = {.key = Key::K, .mod_flags = ModFlags::control},
            .command = [this] { main_window()->zoom(); }
        });
        dockspace.set_left_toolbar({
            ToolbarIconButton("plus")
                .on_click([this] { text = "Clicked toolbar button"; }),
            ToolbarSpacer(), 
            ToolbarIconButton("play")
                .on_click([this] { text = "Clicked toolbar button"; }),
        });
        dockspace.set_center_toolbar({
            ToolbarIconButton("attention")
                .on_click([this] { show_demo_window = true; })
                .enabled([this] { return !show_demo_window; }),
            ToolbarSpacer(),
            ToolbarButton("Other Button"),
            ToolbarIconButton("search")
        });
        dockspace.set_right_toolbar({
            ToolbarIconButton("bell")
                .on_click([this] { 
                    event_emitter.publish(ViewManager::OpenCommand{
                        .name = "ExampleView"
                    });
                }),
            ToolbarSpacer(),
            ToolbarIconButton("plus")
                .on_click([this] { 
                    event_emitter.publish(ViewManager::OpenCommand{
                        .name = "MosaicDemoView"
                    });
                })
        });
        // clang-format on
        g_app = this;
    }

private:
    void frame() override {
        dockspace.display(main_window());
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);
        display_icon_viewer();
        ImGui::ShowMetricsWindow();
    }

    void display_icon_viewer() {
        ImGui::Begin("Icon Viewer");
        auto const& all_icons =
            service_context().font_manager().get_all_icon_codes();
        constexpr int columns = 6;
        constexpr float cell_height = 72.0f;
        if (ImGui::BeginTable("Icons", columns,
                              ImGuiTableFlags_SizingStretchSame |
                                  ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_PadOuterX)) {
            for (auto const& [name, code] : all_icons) {
                ImGui::TableNextColumn();
                auto icon_data = ui::icon_data(name);
                float cell_width = ImGui::GetContentRegionAvail().x;
                float start_x = ImGui::GetCursorPosX();
                // Icon
                float icon_width = 24.0f;
                ImGui::SetCursorPosX(start_x +
                                     (cell_width - icon_width) * 0.5f);
                ui::with_font(icon_data.font, 32.0f, [&] {
                    ImGui::TextEx(icon_data.utf8_rep.data());
                });
                // Name
                ImGui::SetCursorPosX(start_x);
                ImGui::SetNextItemWidth(cell_width);
                ImGui::TextWrapped("%s", name.c_str());
                // Give each cell a consistent minimum height.
                float remaining = cell_height - (ImGui::GetCursorPosY() -
                                                 ImGui::GetCursorPosY());
                (void)remaining;
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

    Dockspace dockspace;
    EventReceiver event_receiver;
    EventEmitter event_emitter;
    std::string text;
    bool show_demo_window = true;
};

int main() {
    ExampleApp app;
    return app.run();
}