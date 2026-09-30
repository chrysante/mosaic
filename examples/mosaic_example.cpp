#include <string>

#include <imgui.h>

#include <mosaic/application/application.h>
#include <mosaic/application/input.h>
#include <mosaic/application/input_event.h>
#include <mosaic/gui/command_palette.h>
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
        command_palette().add(
            {.name = "My Command",
             .key_comb = {.key = Key::K, .mod_flags = ModFlags::control},
             .command = [this] { text = "did run command"; }});
    }

private:
    void frame() override {
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

    EventReceiver event_receiver;
    std::string text;
};

} // namespace

int main() {
    ExampleApp app;
    run_application(app);
}