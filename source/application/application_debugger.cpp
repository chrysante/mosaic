#include "application/application_debugger.h"

#include <ranges>

#include <imgui.h>

#include "application/application.h"
#include "application/application_data.h"
#include "application/window.h"
#include "application/window_impl.h"
#include "magic_enum/magic_enum.hpp"

using namespace mosaic;

void Application::Debugger::display() {
    if (!m_open) return;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Application Debugger", &m_open)) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("DebuggerTabs")) {
        if (ImGui::BeginTabItem("Windows")) {
            display_window_debugger();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input Events")) {
            display_event_debugger();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// =============================================================================
// Window Debugger
// =============================================================================

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

// =============================================================================
// Event Debugger
// =============================================================================

static void display_modifiers(InputEvent const& event) {
    ImGui::TableSetColumnIndex(10);
    std::string text = to_string(event.modifier_flags);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

static void display_mouse_event_base(const MouseEvent& event) {
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("(%.1f, %.1f)", event.location_in_window.x,
                event.location_in_window.y);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("(%.1f, %.1f)", event.location_on_screen.x,
                event.location_on_screen.y);
}

static void display_event(MouseDownEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(5);
    ImGui::TextUnformatted(event.button == MouseButton::left    ? "Left"
                           : event.button == MouseButton::right ? "Right"
                                                                : "Other");
    ImGui::TableSetColumnIndex(7);
    ImGui::Text("%ld", event.click_count);
    display_modifiers(event);
}

void display_event(MouseUpEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(5);
    ImGui::TextUnformatted(event.button == MouseButton::left    ? "Left"
                           : event.button == MouseButton::right ? "Right"
                                                                : "Other");
    display_modifiers(event);
}

static void display_event(MouseDragEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(5);
    ImGui::TextUnformatted(event.button == MouseButton::left    ? "Left"
                           : event.button == MouseButton::right ? "Right"
                                                                : "Other");
    display_modifiers(event);
}

void display_event(MouseMoveEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    display_modifiers(event);
}

void display_event(ScrollEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(9);
    ImGui::TextUnformatted(event.is_trackpad ? "Trackpad" : "Mouse");
    display_modifiers(event);
}

static void display_event(MagnificationEvent const& event) {
    display_mouse_event_base(event);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%.3f", event.magnification);
    display_modifiers(event);
}

static void display_event(KeyDownEvent const& event) {
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%d", static_cast<int>(event.key));
    ImGui::TableSetColumnIndex(8);
    ImGui::Text("%d", event.repeat_count);
    display_modifiers(event);
}

static void display_event(KeyUpEvent const& event) {
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%d", static_cast<int>(event.key));
    display_modifiers(event);
}

void Application::Debugger::display_event_debugger() {
    ImGui::Text("Events: %zu", m_input_queue.size());
    ImGui::SameLine();
    if (ImGui::Button("Clear")) m_input_queue.clear();
    ImGui::Separator();

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (!ImGui::BeginTable("InputEvents", 11, table_flags)) return;

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 35.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 135.0f);
    ImGui::TableSetupColumn("Window", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Screen", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Clicks", ImGuiTableColumnFlags_WidthFixed, 55.0f);
    ImGui::TableSetupColumn("Repeat", ImGuiTableColumnFlags_WidthFixed, 55.0f);
    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 75.0f);
    ImGui::TableSetupColumn("Modifiers", ImGuiTableColumnFlags_WidthFixed,
                            120.0f);

    ImGui::TableHeadersRow();
    std::size_t index = 0;
    for (auto const& event : m_input_queue | std::views::reverse) {
        ImGui::TableNextRow();

        // Index
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%zu", index++);

        // ---------------------------------------------------------
        // Event-specific information
        // ---------------------------------------------------------
        ImGui::TableSetColumnIndex(1);
        std::string_view type_name =
            magic_enum::enum_name(get_rtti(event.base()));
        ImGui::TextUnformatted(type_name.data(),
                               type_name.data() + type_name.size());
        csp::visit(event.base(),
                   [](auto const& event) { display_event(event); });
    }

    ImGui::EndTable();
}
