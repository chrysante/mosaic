#include <mosaic/application/application.h>

namespace {

class ExampleApp : public mosaic::Application {
public:
};

} // namespace

int main() {
    ExampleApp app;
    app.create_window({.position = {200, 200}, .size = {800, 600}});
    mosaic::run_application(app);
}