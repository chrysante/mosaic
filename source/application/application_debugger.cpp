#include "application/application_debugger.h"

#include <imgui.h>

#include "application/application.h"
#include "application/application_data.h"
#include "application/window.h"
#include "application/window_impl.h"

using namespace mosaic;

void Application::Debugger::display() {
    if (!m_open) return;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Application Debugger", &m_open)) {
        ImGui::End();
        return;
    }
    display_window_debugger();
    ImGui::End();
}

static void display_window_info(Window const* window) {
    if (!window) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "NULL");
        return;
    }
    const auto position = window->position();
    const auto size = window->size();
    // -------------------------------------------------------------
    // Main row
    // -------------------------------------------------------------
    ImGui::TableNextRow();
    // Window
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", window->title().c_str());
    ImGui::TextDisabled("%p", static_cast<void const*>(window));
    // Position
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d, %d", position.x, position.y);
    // Size
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d x %d", size.x, size.y);
    // Moving
    ImGui::TableSetColumnIndex(3);
    if (window->is_live_moving())
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "MOVING");
    else ImGui::TextDisabled("no");
    // Resizing
    ImGui::TableSetColumnIndex(4);
    if (window->is_live_resizing())
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "RESIZING");
    else ImGui::TextDisabled("no");
    // ImGui viewport
    ImGui::TableSetColumnIndex(5);
    ImGuiViewport* viewport = nullptr;
    // Adapt this lookup to however your application associates
    // Window with its ImGui viewport.
    //
    // For example:
    // viewport = window->imgui_viewport();
    if (viewport) ImGui::Text("%08X", viewport->ID);
    else ImGui::TextDisabled("none");
    // Viewport position
    ImGui::TableSetColumnIndex(6);
    if (viewport) ImGui::Text("%.0f, %.0f", viewport->Pos.x, viewport->Pos.y);
    else ImGui::TextDisabled("-");
    // Visible
    ImGui::TableSetColumnIndex(7);
    // There isn't a visibility accessor in Window, so leave this
    // out unless you want to add one to the Window interface.
    ImGui::TextDisabled("-");
    ImGui::TableSetColumnIndex(8);
    // There isn't a visibility accessor in Window, so leave this
    // out unless you want to add one to the Window interface.
    ImGui::TextUnformatted(
        internal::WindowImpl::get_native_debug_string(*window).c_str());
}

void Application::Debugger::display_window_debugger() {
    auto const& windows = application->m_data->windows;
    ImGui::Text("Windows");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", windows.size());
    ImGui::Separator();
    if (ImGui::BeginTable("Windows", 9,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Window");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Size");
        ImGui::TableSetupColumn("Moving");
        ImGui::TableSetupColumn("Resizing");
        ImGui::TableSetupColumn("Viewport");
        ImGui::TableSetupColumn("Viewport Pos");
        ImGui::TableSetupColumn("Visible");
        ImGui::TableSetupColumn("Native");
        ImGui::TableHeadersRow();
        for (auto& window : windows)
            display_window_info(window.get());
        ImGui::EndTable();
    }
}