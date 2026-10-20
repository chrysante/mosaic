#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <mosaic/application/application.h>
#include <mosaic/application/application_settings.h>
#include <mosaic/application/input.h>
#include <mosaic/application/window.h>
#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/dockspace.h>
#include <mosaic/gui/font.h>
#include <mosaic/gui/run_mode_controller.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/gui/ui_context.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/view_manager.h>
#include <mosaic/gui/view_registry.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/service_context.h>

using namespace mosaic;

class ExampleView : public View {
public:
    ExampleView() {
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

        if (auto* ui_context = get_current_ui_context()) {
            if (ImGui::Button("Run continuously"))
                run_mode_lease =
                    ui_context->run_mode_controller.enter_continuous_run_mode();
            ImGui::SameLine();
            if (ImGui::Button("Stop")) run_mode_lease.reset();
        }
    }

    RunModeController::Lease run_mode_lease;
};

MOSAIC_REGISTER_VIEW(ExampleView, {.name = "ExampleView"})

static void menubar();

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
        command_palette().add({
            .name = "Toggle Menubar",
            .key_comb = {.key = Key::F10}, 
            .command = [this] { dockspace.toggle_menubar(); }
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
    }

private:
    void frame() override {
        dockspace.display(main_window(), menubar);
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);
        display_icon_viewer();
        if (ImGui::Begin("Application Settings"))
            draw_application_settings(*this);
        ImGui::End();
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

static void menubar() {
    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("New Project", "Ctrl+N");
        ImGui::MenuItem("Open Project...", "Ctrl+O");
        ImGui::MenuItem("Open Recent");
        ImGui::Separator();
        ImGui::MenuItem("Save", "Ctrl+S");
        ImGui::MenuItem("Save As...", "Ctrl+Shift+S");
        ImGui::MenuItem("Save All", "Ctrl+Alt+S");
        ImGui::Separator();

        if (ImGui::BeginMenu("Export")) {
            ImGui::MenuItem("Export as PNG");
            ImGui::MenuItem("Export as JPG");
            ImGui::MenuItem("Export as PDF");
            ImGui::MenuItem("Export as JSON");
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Settings", "Ctrl+,");
        ImGui::MenuItem("Exit", "Alt+F4");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Undo", "Ctrl+Z");
        ImGui::MenuItem("Redo", "Ctrl+Y");
        ImGui::Separator();
        ImGui::MenuItem("Cut", "Ctrl+X");
        ImGui::MenuItem("Copy", "Ctrl+C");
        ImGui::MenuItem("Paste", "Ctrl+V");
        ImGui::MenuItem("Duplicate", "Ctrl+D");
        ImGui::MenuItem("Select All", "Ctrl+A");
        ImGui::Separator();
        ImGui::MenuItem("Find", "Ctrl+F");
        ImGui::MenuItem("Replace", "Ctrl+H");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Toolbar");
        ImGui::MenuItem("Status Bar");
        ImGui::MenuItem("Grid");
        ImGui::MenuItem("Guides");
        ImGui::Separator();

        if (ImGui::BeginMenu("Theme")) {
            ImGui::MenuItem("Dark");
            ImGui::MenuItem("Light");
            ImGui::MenuItem("Classic");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Zoom")) {
            ImGui::MenuItem("50%");
            ImGui::MenuItem("75%");
            ImGui::MenuItem("100%");
            ImGui::MenuItem("150%");
            ImGui::MenuItem("200%");
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        ImGui::MenuItem("Command Palette", "Ctrl+Shift+P");
        ImGui::MenuItem("Search Everything", "Ctrl+Shift+F");
        ImGui::Separator();

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Show Metrics");
            ImGui::MenuItem("Show Debug Log");
            ImGui::MenuItem("Show ID Stack Tool");
            ImGui::MenuItem("Inspect Style");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            ImGui::MenuItem("Build", "Ctrl+B");
            ImGui::MenuItem("Build and Run", "Ctrl+F5");
            ImGui::MenuItem("Clean Build");
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Preferences...");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("New Window");
        ImGui::MenuItem("Reset Layout");
        ImGui::Separator();
        ImGui::MenuItem("Hierarchy");
        ImGui::MenuItem("Inspector");
        ImGui::MenuItem("Console");
        ImGui::MenuItem("Asset Browser");
        ImGui::Separator();

        if (ImGui::BeginMenu("Layout")) {
            ImGui::MenuItem("Default");
            ImGui::MenuItem("Coding");
            ImGui::MenuItem("Debugging");
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Documentation");
        ImGui::MenuItem("Keyboard Shortcuts");
        ImGui::MenuItem("Report a Bug");
        ImGui::Separator();
        ImGui::MenuItem("About");
        ImGui::MenuItem("About Dear ImGui");
        ImGui::EndMenu();
    }
}