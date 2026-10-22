#include "application/debug_tools.h"
#include "gui/command_palette.h"

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
#include "gui/layout.h"
#include "support/enum.h"

using namespace mosaic;

using internal::ApplicationImpl;

static void debug_bool(char const* name, bool value) {
    ImGui::Text("%-26s", name);
    ImGui::SameLine();
    ImGui::TextColored(value ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                             : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "%s", value ? "true" : "false");
};

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

static CommandPalette* g_debug_command_palette;

void mosaic::set_debug_command_palette(CommandPalette& command_palette) {
    g_debug_command_palette = &command_palette;
}

CommandPalette& mosaic::get_debug_command_palette() {
    assert(g_debug_command_palette && "Not set?");
    return *g_debug_command_palette;
}

void ApplicationDebugger::display() {
    if (m_open) display_debuggers();
    if (m_show_window_inspector) display_native_window_style_inspector();
    if (m_show_imgui_debugger) ImGui::ShowMetricsWindow(&m_show_imgui_debugger);
    if (m_show_imgui_log) ImGui::ShowDebugLogWindow(&m_show_imgui_log);
}

void ApplicationDebugger::display_debuggers() {
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Application Debugger", &m_open)) {
        ImGui::Checkbox("ImGui Debugger", &m_show_imgui_debugger);
        ImGui::SameLine();
        ImGui::Checkbox("ImGui Log", &m_show_imgui_log);
        ImGui::SameLine();
        bool debug_overlays = want_debug_overlays();
        if (ImGui::Checkbox("Debug Overlays", &debug_overlays))
            set_want_debug_overlays(debug_overlays);
        ImGui::SameLine();
        ImGui::Checkbox("Window style inspector", &m_show_window_inspector);
        if (ImGui::CollapsingHeader("Windows")) draw_window_debugger();
        if (ImGui::CollapsingHeader("Input Events")) draw_event_debugger();
        if (ImGui::CollapsingHeader("Input")) draw_input_debugger();
        if (ImGui::CollapsingHeader("View Manager"))
            GET_APP_IMPL(app).view_manager.draw_debugger();
        if (ImGui::CollapsingHeader("Frame Scheduler"))
            GET_APP_IMPL(app).frame_scheduler->draw_debugger();
        if (ImGui::CollapsingHeader("Properties Layout"))
            ui::draw_properties_debugger();
    }
    ImGui::End();
}

// =============================================================================
// Window Debugger
// =============================================================================

#define WINDOW_TABLE(X)                                                        \
    X(window, "Window")                                                        \
    X(frame, "Frame")                                                          \
    X(properties, "Properties")                                                \
    X(native, "Native")

namespace {
struct WindowTable {
    enum Column {
#define X(name, label) name,
        WINDOW_TABLE(X)
#undef X
    };
};
} // namespace

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
    ImGui::TableSetColumnIndex(WindowTable::window);
    ImGui::Text("%s", window->title().c_str());
    ImGui::TextDisabled("%p", static_cast<void const*>(window));
    if (window->is_live_moving())
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "MOVING");
    if (window->is_live_resizing())
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "RESIZING");
    // Frame
    ImGui::TableSetColumnIndex(WindowTable::frame);
    ImGui::Text("Pos: %d, %d", position.x, position.y);
    ImGui::Text("Size: %d x %d", size.x, size.y);
    // Native
    ImGui::TableSetColumnIndex(WindowTable::native);
    ui::with_font(ui::font(FontStyle::monospaced), [&] {
        ImGui::TextUnformatted(
            internal::WindowImpl::get_native_debug_string(*window).c_str());
    });
    // Other properties
    ImGui::TableSetColumnIndex(WindowTable::properties);
    debug_bool("Focusable", window->is_focusable());
    debug_bool("Focused", window->is_focused());
    ImGui::Text("Titlebar height: %.1f", window->titlebar_height());
}

void ApplicationDebugger::draw_window_debugger() {
    auto const& windows = ApplicationImpl::get_impl(app).windows;
    ImGui::Text("Windows");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", windows.size());
    ImGui::Separator();
    auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                       ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("Windows", enum_count<WindowTable::Column>,
                          table_flags)) {
#define X(name, label) ImGui::TableSetupColumn(label);
        WINDOW_TABLE(X)
#undef X
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
    X(type,            "Type",              ImGuiTableColumnFlags_WidthFixed, 135.0f) \
    X(location_window, "Location (Window)", ImGuiTableColumnFlags_WidthFixed, 110.0f) \
    X(location_screen, "Location (Screen)", ImGuiTableColumnFlags_WidthFixed, 110.0f) \
    X(offset,          "Offset",            ImGuiTableColumnFlags_WidthFixed, 90.0f)  \
    X(button_key,      "Button/Key",        ImGuiTableColumnFlags_WidthFixed, 90.0f)  \
    X(physical_key,    "Physical Key",      ImGuiTableColumnFlags_WidthFixed, 70.0f)  \
    X(click_count,     "Click Count",       ImGuiTableColumnFlags_WidthFixed, 55.0f)  \
    X(source,          "Source",            ImGuiTableColumnFlags_WidthFixed, 75.0f)  \
    X(modifiers,       "Modifiers",         ImGuiTableColumnFlags_WidthFixed, 120.0f)
// clang-format on

namespace {
struct EventTable {
    enum Column {
#define X(name, _2, _3, _4) name,
        EVENT_TABLE(X)
#undef X
    };
};
} // namespace

static void draw_modifiers(InputEvent const& event) {
    ImGui::TableSetColumnIndex(EventTable::modifiers);
    std::string text = to_string(event.modifier_flags);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

static void draw_mouse_event_base(const MouseEvent& event) {
    ImGui::TableSetColumnIndex(EventTable::location_window);
    ImGui::Text("(%.1f, %.1f)", event.location_in_window.x,
                event.location_in_window.y);
    ImGui::TableSetColumnIndex(EventTable::location_screen);
    ImGui::Text("(%.1f, %.1f)", event.location_on_screen.x,
                event.location_on_screen.y);
}

static void draw_event(MouseDownEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    ImGui::TableSetColumnIndex(EventTable::click_count);
    ImGui::Text("%ld", event.click_count);
    draw_modifiers(event);
}

void draw_event(MouseUpEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    draw_modifiers(event);
}

static void draw_event(MouseDragEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(EventTable::button_key);
    ImGui::Text("%s", to_string(event.button).c_str());
    draw_modifiers(event);
}

void draw_event(MouseMoveEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    draw_modifiers(event);
}

void draw_event(ScrollEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::offset);
    ImGui::Text("(%.1f, %.1f)", event.offset.x, event.offset.y);
    ImGui::TableSetColumnIndex(EventTable::source);
    ImGui::TextUnformatted(event.is_trackpad ? "Trackpad" : "Mouse");
    draw_modifiers(event);
}

static void draw_event(MagnificationEvent const& event) {
    draw_mouse_event_base(event);
    ImGui::TableSetColumnIndex(EventTable::offset);
    ImGui::Text("%.3f", event.magnification);
    draw_modifiers(event);
}

static void draw_event(KeyEvent const& event) {
    ImGui::TableSetColumnIndex(EventTable::button_key);
    ImGui::Text("%s", to_string(event.key).c_str());
    if (auto* key_down = csp::dyncast<KeyDownEvent const*>(&event);
        key_down && key_down->is_repeat) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(32, 225, 32, 255));
        ImGui::Text("REPEAT");
        ImGui::PopStyleColor();
    }
    ImGui::TableSetColumnIndex(EventTable::physical_key);
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
        ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("InputEvents", enum_count<EventTable::Column>,
                           table_flags))
        return;

#define X(name, label, flags, width)                                           \
    ImGui::TableSetupColumn(label, flags, width);
    EVENT_TABLE(X)
#undef X

    ImGui::TableHeadersRow();
    for (auto const& event : m_input_queue | std::views::reverse) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(EventTable::type);
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
    if (ImGui::TreeNodeEx("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        ImGui::TreePop();
    }

    ImGui::Spacing();

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------
    if (ImGui::TreeNodeEx("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        ImGui::TreePop();
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