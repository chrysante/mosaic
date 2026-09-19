#include <imgui.h>
#include <iostream>
#include <mosaic/application/application.h>

namespace {

class ExampleApp : public mosaic::Application {
public:
private:
    void frame() override {
        ImGui::ShowDemoWindow();
        std::cout << "Frame\n";
    }
};

} // namespace

int main() {
    ExampleApp app;
    mosaic::run_application(app);
}