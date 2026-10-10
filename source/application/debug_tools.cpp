#include "application/debug_tools.h"

#ifndef NDEBUG

#include <ranges>

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include "application/application.h"
#include "application/application_impl.h"
#include "application/debug_settings.h"
#include "application/input_event.h"
#include "application/window.h"
#include "application/window_impl.h"
#include "gui/font.h"
#include "support/enum.h"

using namespace mosaic;

using internal::ApplicationImpl;

static bool& want_debug_overlays_flag() {
    static bool flag;
    return flag;
}

bool mosaic::want_debug_overlays() { return want_debug_overlays_flag(); }

void mosaic::set_want_debug_overlays(bool value) {
    want_debug_overlays_flag() = value;
}

void mosaic::toggle_want_debug_overlays() {
    want_debug_overlays_flag() ^= true;
}

void ApplicationDebugger::display() {
    if (!m_open) return;
    display_debuggers();
    display_native_window_style_inspector();
}

void ApplicationDebugger::display_debuggers() {
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Application Debugger", &m_open)) {
        if (ImGui::BeginTabBar("DebuggerTabs")) {
            if (ImGui::BeginTabItem("Windows")) {
                draw_window_debugger();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Input Events")) {
                draw_event_debugger();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Input")) {
                draw_input_debugger();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// =============================================================================
// Window Debugger
// =============================================================================

static void draw_window_info(Window const* window) {
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
    if (window->is_focused())
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(50, 100, 180, 80));
    // Window
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", window->title().c_str());
    ImGui::TextDisabled("%p", static_cast<void const*>(window));
    if (window->is_live_moving())
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "MOVING");
    if (window->is_live_resizing())
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "RESIZING");
    // Frame
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Pos: %d, %d", position.x, position.y);
    ImGui::Text("Size: %d x %d", size.x, size.y);
    // Native
    ImGui::TableSetColumnIndex(2);
    ui::with_font(ui::font(FontStyle::monospaced), [&] {
        ImGui::TextUnformatted(
            internal::WindowImpl::get_native_debug_string(*window).c_str());
    });
    // ImGui viewport
    ImGui::TableSetColumnIndex(3);
    ImGuiViewport* viewport = nullptr;
    // Adapt this lookup to however your application associates
    // Window with its ImGui viewport.
    //
    // For example:
    // viewport = window->imgui_viewport();
    if (viewport) ImGui::Text("%08X", viewport->ID);
    else ImGui::TextDisabled("none");
    // Viewport position
    ImGui::TableSetColumnIndex(4);
    if (viewport) ImGui::Text("%.0f, %.0f", viewport->Pos.x, viewport->Pos.y);
    else ImGui::TextDisabled("-");
    // Visible
    ImGui::TableSetColumnIndex(5);
    // There isn't a visibility accessor in Window, so leave this
    // out unless you want to add one to the Window interface.
    ImGui::TextDisabled("-");
}

void ApplicationDebugger::draw_window_debugger() {
    auto const& windows = ApplicationImpl::get_impl(app).windows;
    ImGui::Text("Windows");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", windows.size());
    ImGui::Separator();
    auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_Resizable |
                       ImGuiTableFlags_SizingStretchProp |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX;
    if (ImGui::BeginTable("Windows", 6, table_flags)) {
        ImGui::TableSetupColumn("Window");
        ImGui::TableSetupColumn("Frame");
        ImGui::TableSetupColumn("Native");
        ImGui::TableSetupColumn("Viewport");
        ImGui::TableSetupColumn("Viewport Pos");
        ImGui::TableSetupColumn("Visible");
        ImGui::TableHeadersRow();
        for (auto& window : windows)
            draw_window_info(window.get());
        ImGui::EndTable();
    }
}

// =============================================================================
// Event Debugger
// =============================================================================

// clang-format off
#define EVENT_TABLE(X)                                                                    \
    X(col_type,            "Type",              ImGuiTableColumnFlags_WidthFixed, 135.0f) \
    X(col_location_window, "Location (Window)", ImGuiTableColumnFlags_WidthFixed, 110.0f) \
    X(col_location_screen, "Location (Screen)", ImGuiTableColumnFlags_WidthFixed, 110.0f) \
    X(col_offset,          "Offset",            ImGuiTableColumnFlags_WidthFixed, 90.0f)  \
    X(col_button_key,      "Button/Key",        ImGuiTableColumnFlags_WidthFixed, 90.0f)  \
    X(col_physical_key,    "Physical Key",      ImGuiTableColumnFlags_WidthFixed, 70.0f)  \
    X(col_click_count,     "Click Count",       ImGuiTableColumnFlags_WidthFixed, 55.0f)  \
    X(col_source,          "Source",            ImGuiTableColumnFlags_WidthFixed, 75.0f)  \
    X(col_modifiers,       "Modifiers",         ImGuiTableColumnFlags_WidthFixed, 120.0f)
// clang-format on

enum EventColName {
#define X(name, _2, _3, _4) name,
    EVENT_TABLE(X)
#undef X
};

static void draw_modifiers(InputEvent const& event) {
    ImGui::TableSetColumnIndex(col_modifiers);
    std::string text = to_string(event.modifier_flags);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

static void draw_mouse_event_base(const MouseEvent& event) {
    ImGui::TableSetColumnIndex(col_location_window);
    ImGui::Text("(%.1f, %.1f)", event.location_in_window.x,
                event.location_in_window.y);
    ImGui::TableSetColumnIndex(col_location_screen);
    ImGui::Text("(%.1f, %.1f)", event.location_on_screen.x,
                event.location_on_screen.y);
}

static void draw_event(MouseDownEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    ImGui::TableSetColumnIndex(col_click_count);
    ImGui::Text("%ld", event.click_count);
    draw_modifiers(event);
}

void draw_event(MouseUpEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    draw_modifiers(event);
}

static void draw_event(MouseDragEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(col_button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    draw_modifiers(event);
}

void draw_event(MouseMoveEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    draw_modifiers(event);
}

void draw_event(ScrollEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(col_source);
    ImGui::TextUnformatted(event.is_trackpad ? "Trackpad" : "Mouse");
    draw_modifiers(event);
}

static void draw_event(MagnificationEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(col_offset);
    ImGui::Text("%.3f", event.magnification);
    draw_modifiers(event);
}

static void draw_event(KeyEvent const& event) {
    ImGui::TableSetColumnIndex(col_button_key);
    ImGui::Text("%s", to_string(event.key).c_str());
    if (auto* key_down = csp::dyncast<KeyDownEvent const*>(&event);
        key_down && key_down->is_repeat) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(32, 225, 32, 255));
        ImGui::Text("REPEAT");
        ImGui::PopStyleColor();
    }
    ImGui::TableSetColumnIndex(col_physical_key);
    ImGui::Text("%s", to_string(event.physical).c_str());
    draw_modifiers(event);
}

static void draw_event(ModifiersChangedEvent const& event) {
    draw_modifiers(event);
}

void ApplicationDebugger::draw_event_debugger() {
    static ImGuiTableFlags const table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("InputEvents", enum_count<EventColName>,
                           table_flags))
        return;

#define X(name, label, flags, width)                                           \
    ImGui::TableSetupColumn(label, flags, width);
    EVENT_TABLE(X)
#undef X

    ImGui::TableHeadersRow();
    for (auto const& event : m_input_queue | std::views::reverse) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(col_type);
        std::string_view type_name =
            magic_enum::enum_name(get_rtti(event.base()));
        ImGui::TextUnformatted(type_name.data(),
                               type_name.data() + type_name.size());
        csp::visit(event.base(), [](auto const& event) { draw_event(event); });
    }

    ImGui::EndTable();
}

// =============================================================================
// Input Debugger
// =============================================================================

void ApplicationDebugger::draw_input_debugger() {
    Input const& input = app.input();
    // -------------------------------------------------------------------------
    // Mouse
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
        vml::float2 const mouse_pos = input.mouse_position();
        vml::float2 const mouse_offset = input.mouse_offset();
        vml::float2 const scroll = input.scroll_offset();

        ImGui::Text("Position");
        ImGui::SameLine(150.0f);
        ImGui::Text("(%.1f, %.1f)", mouse_pos.x, mouse_pos.y);

        ImGui::Text("Offset");
        ImGui::SameLine(150.0f);
        ImGui::TextColored((mouse_offset.x != 0.0f || mouse_offset.y != 0.0f)
                               ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f)
                               : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "(%.1f, %.1f)", mouse_offset.x, mouse_offset.y);

        ImGui::Text("Scroll");
        ImGui::SameLine(150.0f);
        ImGui::TextColored((scroll.x != 0.0f || scroll.y != 0.0f)
                               ? ImVec4(0.3f, 0.8f, 1.0f, 1.0f)
                               : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "(%.1f, %.1f)", scroll.x, scroll.y);

        ImGui::Spacing();

        // Mouse buttons
        ImGui::Text("Buttons");
        ImGui::Separator();
        for (size_t i = 0; i < enum_count<MouseButton>; ++i) {
            MouseButton button = static_cast<MouseButton>(i);
            auto button_name = to_string(button);
            ImGui::TextUnformatted(button_name.data(),
                                   button_name.data() + button_name.size());
            if (input.mouse_down(button)) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
                ImGui::Text("[DOWN]");
                ImGui::PopStyleColor();
            }
        }
    }

    ImGui::Spacing();

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Modifiers");
        ImGui::Separator();
        auto modifier = [](bool active, char const* name) {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  active ? ImVec4(0.2f, 0.55f, 0.3f, 1.0f)
                                         : ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  active ? ImVec4(0.25f, 0.65f, 0.35f, 1.0f)
                                         : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::SmallButton(name);
            ImGui::PopStyleColor(2);
        };
        ModFlags const mods = input.mod_flags();
        modifier((mods & ModFlags::caps_lock) != ModFlags::none, "Caps Lock");
        modifier((mods & ModFlags::shift) != ModFlags::none, "Shift");
        modifier((mods & ModFlags::control) != ModFlags::none, "Ctrl");
        modifier((mods & ModFlags::alt) != ModFlags::none, "Alt");
        modifier((mods & ModFlags::super) != ModFlags::none, "Super");
        modifier((mods & ModFlags::num_lock) != ModFlags::none, "Num Lock");

        ImGui::Spacing();
        ImGui::Text("Flags");
        ImGui::SameLine(150.0f);
        ImGui::Text("%s", to_string(mods).c_str());

        ImGui::Spacing();
        ImGui::Text("Pressed keys");
        ImGui::Separator();
        bool any_key_down = false;
        for (size_t i = 0; i < enum_count<PhysicalKey>; ++i) {
            auto key = static_cast<PhysicalKey>(i);
            if (!input.key_down(key)) continue;
            any_key_down = true;
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("%s", to_string(key).c_str());
            ImGui::PopStyleColor();
        }
        if (!any_key_down) ImGui::TextDisabled("No keys pressed");
    }

    ImGui::Spacing();

    // -------------------------------------------------------------------------
    // Activity indicator
    // -------------------------------------------------------------------------
    bool const mouse_moved =
        input.mouse_offset().x != 0.0f || input.mouse_offset().y != 0.0f;

    bool const scrolled =
        input.scroll_offset().x != 0.0f || input.scroll_offset().y != 0.0f;

    bool any_mouse_down = false;
    for (size_t i = 0; i < enum_count<MouseButton>; ++i) {
        if (input.mouse_down(static_cast<MouseButton>(i))) {
            any_mouse_down = true;
            break;
        }
    }

    bool any_key_down = false;
    for (size_t i = 0; i < enum_count<PhysicalKey>; ++i) {
        if (input.key_down(static_cast<PhysicalKey>(i))) {
            any_key_down = true;
            break;
        }
    }

    ImGui::Separator();
    ImGui::Text("Input activity");

    ImGui::SameLine();

    if (mouse_moved || scrolled || any_mouse_down || any_key_down)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "ACTIVE");
    else ImGui::TextDisabled("Idle");
}

#endif // NDEBUG