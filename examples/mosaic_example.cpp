#include "mosaic/gui/toolbar.h"
#include <string>

#include <imgui.h>

#include <mosaic/application/application.h>
#include <mosaic/application/input.h>
#include <mosaic/application/input_event.h>
#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/dockspace.h>
#include <mosaic/gui/font.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/font_manager.h>

using namespace mosaic;

namespace {

class ExampleApp : public Application {
public:
    ExampleApp()
        : event_receiver(make_receiver()) {
        event_receiver.listen([this](InputEvent& e) {
            e.handle<InputEventMask::key_down>([this](KeyEvent event) {
                text = std::to_string((int)event.key);
            });
        });
        // clang-format off
        command_palette().add({
            .name = "My Command",
            .key_comb = {.key = Key::K, .mod_flags = ModFlags::control},
            .command = [this] { text = "did run command"; }
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
                .on_click([this] { text = "Clicked toolbar button"; })
        });
        // clang-format on
        main_window()->set_style({.has_toolbar_area = true});
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

        auto& fm = font_manager();
        auto icon = fm.get_icon(IconSize::_24, "plus-circled");

        ImGui::PushFont(icon.font);
        ImGui::Button(icon.utf8_rep.data());
        ImGui::PopFont();

        ImGui::Text("%s", text.c_str());

        ImGui::End();
    }

    Dockspace dockspace;
    EventReceiver event_receiver;
    std::string text;
};

} // namespace

int main() {
    ExampleApp app;
    run_application(app);
}