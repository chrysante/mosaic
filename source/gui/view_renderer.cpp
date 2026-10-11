#include "gui/view_renderer.h"

#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>
#include <vml/vml.hpp>

#include "application/debug_settings.h"
#include "application/window.h"
#include "application/window_desc.h"
#include "gui/view.h"
#include "gui/view_manager.h"
#include "vml/shapes.hpp"

using namespace mosaic;
using namespace vml::short_types;

static void debug_bool(char const* name, bool value) {
    ImGui::Text("%-26s", name);
    ImGui::SameLine();
    ImGui::TextColored(value ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                             : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "%s", value ? "true" : "false");
};

static void append_to_debugger(auto&& f) {
    if (!want_debug_overlays()) return;
    if (ImGui::Begin("View Debugger")) f();
    ImGui::End();
}

static Window* get_window_from_viewport(ImGuiViewport* viewport) {
    return static_cast<Window*>(viewport->PlatformHandle);
}

static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

static void dock_titlebar_behaviour(ImRect toolbar_bb, bool& dragging) {
    ImGuiContext& g = *GImGui;
    auto* window = ImGui::GetCurrentWindow();
    if (window->Viewport != ImGui::GetMainViewport()) {
        dragging &= ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        return;
    }
    if (!dragging) {
        ImGuiID const id = window->GetID("##UndockDrag");
        if (!ImGui::ItemAdd(toolbar_bb, id)) return;
        bool hovered = false;
        bool held = false;
        ImGui::ButtonBehavior(toolbar_bb, id, &hovered, &held,
                              ImGuiButtonFlags_MouseButtonLeft);
        dragging = held && g.ActiveId == id;
    }
    dragging &= ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    if (dragging && window->DockNode && g.MovingWindow != window) {
        ImGui::DockContextQueueUndockWindow(&g, window);
        ImGui::StartMouseMovingWindow(window);
        // Match the override used by ImGui's docking code.
        g.MovingWindow = window;
        // Correct the offset: use the docked window's position, not the root
        // dock host's position.
        g.ActiveIdClickOffset =
            (float2)g.IO.MouseClickedPos[0] - (float2)window->Pos;
    }
}

struct mosaic::RuntimeViewState {
    struct Frame {
        bool focused = false;
        bool hovered = false;
        bool docked = false;
        bool wants_native_toolbar = false;

        //
        bool docked_last_frame = false;
    } frame;

    struct Persistent {
        bool apply_initial_geometry = true;
        bool window_dragging = false;
    } persistent;
};

ImGuiViewRenderer::ImGuiViewRenderer() = default;

ImGuiViewRenderer::~ImGuiViewRenderer() = default;

void ImGuiViewRenderer::begin_frame() {
    for (auto& [view, state] : m_runtime) {
        bool docked_last_frame = state.frame.docked;
        state.frame = {};
        state.frame.docked_last_frame = docked_last_frame;
    }
    m_viewports_with_native_toolbar.clear();
}

static std::string make_imgui_id(View const& view) {
    auto id = utl::strcat(view.title(), "###", view.type_name(), "_",
                          view.id().instance);
    return id;
}

namespace {

struct ToolbarGeometry {
    vml::float2 position;
    vml::float2 size;
    vml::float2 padding;
};

} // namespace

static ToolbarGeometry compute_toolbar_geometry(RuntimeViewState state) {
    float2 padding = {6, 6};

    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* window = get_window_from_viewport(imgui_window->Viewport);
    float width = imgui_window->Size.x;
    if (!window || !state.frame.wants_native_toolbar)
        return {.position = {0.0f, 0.0f},
                .size = {width, 25.0f},
                .padding = padding};
    auto height = window->titlebar_height() - 2.0f * padding.y;
    float left_inset = compute_left_inset(*window);
    return {.position = {left_inset, 0.0f},
            .size = {width - left_inset, height},
            .padding = padding};
}

static void display_toolbar(Toolbar& toolbar, ToolbarGeometry geometry,
                            RuntimeViewState state) {
    toolbar.set_padding(geometry.padding);
    toolbar.set_height(geometry.size.y);

    vml::float2 screen_pos = (float2)ImGui::GetWindowPos() + geometry.position;
    ImGui::SetCursorScreenPos(screen_pos);
    toolbar.display(geometry.size.x);

    ImGui::SetCursorPos({0, geometry.position.y + toolbar.get_height() + 1});
    if (state.frame.docked || state.persistent.window_dragging) {
        ImRect toolbar_rect(screen_pos, screen_pos + geometry.size);
        dock_titlebar_behaviour(toolbar_rect, state.persistent.window_dragging);
    }
}

void ImGuiViewRenderer::render_view(View& view) {
    auto layout = view.layout();
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    if (view.toolbar()) {
        flags |= ImGuiWindowFlags_NoTitleBar;
        auto const& state = runtime_state(view);
        if (!state.frame.docked_last_frame && !state.persistent.window_dragging)
            flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowSizeConstraints(layout.min_size, {FLT_MAX, FLT_MAX});
    auto& persistent = runtime_state(view).persistent;
    if (persistent.apply_initial_geometry) {
        ImGui::SetNextWindowPos(layout.position);
        ImGui::SetNextWindowSize(layout.size);
        persistent.apply_initial_geometry = false;
    }

    vml::float2 view_padding = view.padding();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, view_padding);

    bool open_state = true;
    bool visible =
        ImGui::Begin(make_imgui_id(view).c_str(), &open_state, flags);

    ImGui::PopStyleVar();

    if (!open_state) view.request_close();

    auto& state = update_and_get_runtime_state(view);
    if (visible) {
        auto geometry = view.toolbar() ? compute_toolbar_geometry(state)
                                       : ToolbarGeometry{};
        float toolbar_height =
            view.toolbar() ? geometry.size.y + 2 * geometry.padding.y
                           : ImGui::GetCurrentWindowRead()->TitleBarHeight;
        float2 cursor_pos = float2{0, toolbar_height} + view_padding;
        ImGui::SetCursorPos(cursor_pos);
        view.display();
        if (view.toolbar()) display_toolbar(*view.m_toolbar, geometry, state);
    }
    ImGui::End();
}

// This must be called after ImGui::Begin()
RuntimeViewState&
ImGuiViewRenderer::update_and_get_runtime_state(View const& view) {
    auto& state = runtime_state(view);
    // This is the first point at which the ImGuiWindow belongs to us.
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* viewport = imgui_window->Viewport;
    auto& frame = state.frame;
    frame.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
                    !ImGui::FindBlockingModal(imgui_window);
    frame.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    frame.docked = imgui_window->DockIsActive;
    bool in_main_viewport = viewport == ImGui::GetMainViewport();
    frame.wants_native_toolbar =
        !in_main_viewport && imgui_window == imgui_window->Viewport->Window &&
        view.toolbar() != nullptr;
    if (frame.wants_native_toolbar)
        m_viewports_with_native_toolbar.insert(viewport);
    return state;
}

static WindowStyle make_window_style(bool wants_toolbar) {
    if (wants_toolbar)
        return {
            .decoration = WindowDecoration::normal,
            .full_size_content = true,
            .large_titlebar = true,
        };
    else
        return {
            .decoration = WindowDecoration::none,
            .full_size_content = false,
            .large_titlebar = false,
        };
}

void ImGuiViewRenderer::end_frame() {
    auto& g = *GImGui;
    for (int i = 1; i < g.Viewports.size(); ++i) {
        auto* viewport = g.Viewports[i];
        auto* window = (Window*)viewport->PlatformHandle;
        if (!window) continue;
        bool wants_toolbar = m_viewports_with_native_toolbar.contains(viewport);
        window->set_style(make_window_style(wants_toolbar));
    }
}

RuntimeViewState& ImGuiViewRenderer::runtime_state(View const& view) {
    return m_runtime[&view];
}