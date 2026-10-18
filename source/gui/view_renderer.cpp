#include "gui/view_renderer.h"

#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/debug_settings.h"
#include "application/window.h"
#include "application/window_desc.h"
#include "gui/toolbar.h"
#include "gui/toolbar_renderer.h"
#include "gui/view.h"
#include "gui/view_manager.h"

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
        ToolbarRenderer toolbar_renderer;
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
                          view.view_id().instance);
    return id;
}

static ToolbarGeometry compute_toolbar_geometry(RuntimeViewState state,
                                                ToolbarStyle const& style) {
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* window = get_window_from_viewport(imgui_window->Viewport);
    float width = imgui_window->Size.x;
    float offset_y = ImGui::GetCursorPosY() - imgui_window->WindowPadding.y;
    ToolbarPadding padding = resolve_toolbar_padding(style.padding);
    if (!window || !state.frame.wants_native_toolbar)
        return {.position = {0.0f, offset_y},
                .size = {width, 38.0f},
                .padding = padding};
    auto height = window->titlebar_height();
    float left_inset = compute_left_inset(*window);
    return {.position = {left_inset, offset_y},
            .size = {width - left_inset, height},
            .padding = padding};
}

static void display_toolbar(ToolbarRenderer& toolbar_renderer,
                            ToolbarStyle const& style, ToolbarGeometry geometry,
                            RuntimeViewState state) {
    toolbar_renderer.display(style, geometry);
    ImGui::SetCursorPos({0, geometry.position.y + geometry.size.y + 1});
    if (state.frame.docked || state.persistent.window_dragging) {
        vml::float2 screen_pos =
            (vml::float2)ImGui::GetWindowPos() + geometry.position;
        ImRect toolbar_rect(screen_pos, screen_pos + geometry.size);
        dock_titlebar_behaviour(toolbar_rect, state.persistent.window_dragging);
    }
}

static ImGuiWindowClass window_class = [] {
    ImGuiWindowClass window_class;
    window_class.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration;
    return window_class;
}();

ImGuiViewRenderer::Result ImGuiViewRenderer::render_view(View& view) {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    if (view.uses_toolbar()) {
        flags |= ImGuiWindowFlags_NoTitleBar;
        auto const& state = runtime_state(view);
        if (!state.frame.docked_last_frame && !state.persistent.window_dragging)
            flags |= ImGuiWindowFlags_NoMove;
    }

    ImGui::SetNextWindowClass(&window_class);
    ImGui::SetNextWindowSizeConstraints(view.view_class().min_size,
                                        {FLT_MAX, FLT_MAX});
    ImGui::SetNextWindowPos(view.view_class().default_position,
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(view.view_class().default_size,
                             ImGuiCond_FirstUseEver);
    vml::float2 view_padding = view.padding();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, view_padding);
    std::string view_id = make_imgui_id(view);
    bool open_state = true;
    bool visible = ImGui::Begin(view_id.c_str(), &open_state, flags);
    utl::scope_guard guard = [] { ImGui::End(); };
    ImGui::PopStyleVar();
    if (!open_state) view.request_close();
    auto& state = update_and_get_runtime_state(view);
    if (!visible) return {};
    if (!view.uses_toolbar()) {
        float title_height = ImGui::GetCurrentWindowRead()->TitleBarHeight;
        vml::float2 content_pos = float2{0.0f, title_height};
        ImGui::SetCursorPos(content_pos + view_padding);
        view.display();
    } else {
        ToolbarStyle style = view.m_toolbar_style ? *view.m_toolbar_style
                                                  : get_global_toolbar_style();
        auto geometry = compute_toolbar_geometry(state, style);
        vml::float2 content_pos = {0.0f, geometry.position.y + geometry.size.y};
        ImGui::SetCursorPos(content_pos + view_padding);
        view.display();
        display_toolbar(state.persistent.toolbar_renderer, style, geometry,
                        state);
    }
    return {.focused = state.frame.focused};
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
        view.uses_toolbar();
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
            .full_size_content = true,
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
    auto itr = m_runtime.find(&view);
    if (itr == m_runtime.end()) {
        itr = m_runtime.insert({&view, RuntimeViewState{}}).first;
        itr->second.persistent.toolbar_renderer.set_toolbar(&view.toolbar());
    }
    return itr->second;
}