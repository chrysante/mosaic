#include <mosaic/application/application.h>

int main() {
  mosaic::Application app;
  app.create_window({.position = {200, 200}, .size = {800, 600}});
  mosaic::run_application(app);
}