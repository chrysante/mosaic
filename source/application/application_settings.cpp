#include "application/application_settings.h"

#include <imgui.h>

#include "application/application.h"
#include "service/service_context.h"
#include "service/style_system.h"
#include "support/file_dialog.h"

using namespace mosaic;

static void draw_gpu_settings(Application&) { ImGui::TextDisabled("Empty"); }

static void draw_style_system_settings(StyleSystem& style_system) {
    ImGui::Text("Active Style: \"%s\"",
                style_system.active_style_path().c_str());
    if (ImGui::Button("Load Style")) {
        show_open_panel(
            {}, [&style_system](std::span<std::filesystem::path const> paths) {
                style_system.load_style(paths.front());
            });
    }
}

void mosaic::draw_application_settings(Application& app) {
    if (ImGui::CollapsingHeader("GPU")) draw_gpu_settings(app);
    if (ImGui::CollapsingHeader("Style System"))
        draw_style_system_settings(app.service_context().style_system());
}