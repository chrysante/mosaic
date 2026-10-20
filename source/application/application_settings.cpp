#include "application/application_settings.h"

#include <imgui.h>

#include "application/application.h"
#include "application/application_impl.h"

using namespace mosaic;

static void draw_gpu_settings(Application& app) {
    ImGui::TextDisabled("Empty");
}

void mosaic::draw_application_settings(Application& app) {
    if (ImGui::CollapsingHeader("GPU")) draw_gpu_settings(app);
}