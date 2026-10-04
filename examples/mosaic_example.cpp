#include "mosaic/gui/toolbar.h"
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <mosaic/application/application.h>
#include <mosaic/application/input.h>
#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/dockspace.h>
#include <mosaic/gui/font.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/view_manager.h>
#include <mosaic/gui/view_registry.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/service_context.h>

using namespace mosaic;

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
    }
};

MOSAIC_REGISTER_VIEW(ExampleView, "ExampleView")

static bool full_size = true;

static void test_view() {
    ImGuiWindow* window = ImGui::FindWindowByName("test-view");
    bool docked = window && window->DockNode != nullptr;
    ImGui::SetNextWindowFlags(ImGuiWindowFlags_NoMove, !docked);
    ImGui::Begin("test-view");
    if (auto* viewport = ImGui::GetCurrentWindow()->Viewport) {
        if (auto* window = (Window*)viewport->PlatformHandle) {
            window->set_style(
                {.full_size_content = full_size, .large_titlebar = full_size});
        }
    }
    ImGui::Checkbox("Full Size", &full_size);
    ImGui::End();
}

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
            .name = "My Command",
            .key_comb = {.key = Key::F, .mod_flags = ModFlags::control},
            .command = [] { full_size = !full_size; }
        });
        dockspace.set_left_toolbar({
            ToolbarIconButton("plus")
                .on_click([this] { text = "Clicked toolbar button"; })
        });
        dockspace.set_center_toolbar({
            ToolbarIconButton("info")
                .on_click([this] { text = "Clicked toolbar button"; })
        });
        dockspace.set_right_toolbar({
            ToolbarIconButton("bell")
                .on_click([this] { 
                    text = "Clicked toolbar button"; 
                    event_emitter.publish(ViewManager::OpenCommand{
                        .name = "ExampleView"
                    });
                })
        });
        // clang-format on
    }

private:
    void frame() override {
        dockspace.display(main_window());

        ImGui::ShowDemoWindow();

        ImGui::Begin("Hello");
        auto* font =
            ui::font(FontSize::Large, FontWeight::bold, FontStyle::monospaced);
        ImGui::PushFont(font);
        ImGui::Button("My Button");
        ImGui::PopFont();
        auto& fm = service_context().font_manager();
        auto icon = fm.get_icon(IconSize::_24, "plus-circled");
        ImGui::PushFont(icon.font);
        ImGui::Button(icon.utf8_rep.data());
        ImGui::PopFont();
        ImGui::Text("%s", text.c_str());
        ImGui::End();

        test_view();

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
                auto icon_data = ui::icon_data({IconSize::_24}, name);
                float cell_width = ImGui::GetContentRegionAvail().x;
                float start_x = ImGui::GetCursorPosX();
                // Icon
                float icon_width = 24.0f;
                ImGui::SetCursorPosX(start_x +
                                     (cell_width - icon_width) * 0.5f);
                ui::with_font(icon_data.font, [&] {
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
};

int main() {
    ExampleApp app;
    return app.run();
}