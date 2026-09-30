#include <imgui.h>

#include <mosaic/application/application.h>
#include <mosaic/application/font_manager.h>
#include <mosaic/gui/font.h>

using namespace mosaic;

namespace {

class ExampleApp : public Application {
public:
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

        ImGui::End();
    }
};

} // namespace

int main() {
    ExampleApp app;
    run_application(app);
}