#include "gui/panel_renderer.h"

#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/window.h"
#include "application/window_desc.h"
#include "gui/panel.h"
#include "gui/toolbar.h"
#include "gui/view.h"
#include "support/geometry.h"

using namespace mosaic;
using namespace vml::short_types;

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

struct mosaic::RuntimePanelState {
    struct Transient {
        bool focused = false;
        bool hovered = false;
        bool docked = false;
        bool wants_native_toolbar = false;

        //
        bool docked_last_frame = false;
    } transient;

    struct Persistent {
        bool apply_initial_geometry = true;
        bool window_dragging = false;
    } persistent;
};

PanelRenderer::PanelRenderer() = default;

PanelRenderer::~PanelRenderer() = default;

void PanelRenderer::begin_frame() {
    for (auto& [view, state] : m_runtime) {
        bool docked_last_frame = state.transient.docked;
        state.transient = {};
        state.transient.docked_last_frame = docked_last_frame;
    }
    m_viewports_with_native_toolbar.clear();
}

static std::string make_imgui_id(Panel const& view) {
    return utl::strcat(view.title(), "##", (void const*)&view);
}

static Rect compute_toolbar_frame(RuntimePanelState state) {
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* window = get_window_from_viewport(imgui_window->Viewport);
    float width = imgui_window->Size.x;
    float offset_y = ImGui::GetCursorPosY() - imgui_window->WindowPadding.y;
    if (!window || !state.transient.wants_native_toolbar) {
        return {{imgui_window->Pos.x, imgui_window->Pos.y + offset_y},
                {width, 38.0f}};
    }
    auto height = window->titlebar_height();
    float left_inset = compute_left_inset(*window);
    return {
        {imgui_window->Pos.x + left_inset, imgui_window->Pos.y + offset_y},
        {width - left_inset, height},
    };
}

static void display_toolbar(ui::View* toolbar, Rect frame,
                            RuntimePanelState state) {
    ui::internal::set_toolbar_padding(toolbar, {6.0f, 6.0f, 6.0f, 6.0f});
    toolbar->display(frame);
    float separator_size = ImGui::GetStyle().SeparatorSize;
    vml::float2 line_min = {ImGui::GetWindowPos().x,
                            frame.origin.y + frame.size.y};
    vml::float2 line_max =
        line_min + vml::float2{ImGui::GetWindowSize().x, separator_size};
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddRectFilled(line_min, line_max,
                      ImGui::GetColorU32(ImGuiCol_Separator));
    if (state.transient.docked || state.persistent.window_dragging) {
        ImRect toolbar_rect(frame.origin, frame.origin + frame.size);
        dock_titlebar_behaviour(toolbar_rect, state.persistent.window_dragging);
    }
}

static ImGuiWindowClass window_class = [] {
    ImGuiWindowClass window_class;
    window_class.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration;
    return window_class;
}();

PanelRenderer::Result PanelRenderer::render_panel(Panel& panel) {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    bool const uses_toolbar = panel.toolbar() != nullptr;
    if (uses_toolbar) {
        flags |= ImGuiWindowFlags_NoTitleBar;
        auto const& state = runtime_state(panel);
        if (!state.transient.docked_last_frame &&
            !state.persistent.window_dragging)
            flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowClass(&window_class);
    auto const& geometry = panel.geometry();
    ImGui::SetNextWindowSizeConstraints(geometry.size_constraints.min_size,
                                        geometry.size_constraints.max_size);
    ImGui::SetNextWindowPos(geometry.frame.origin, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(geometry.frame.size, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    std::string imgui_id = make_imgui_id(panel);
    bool open_state = true;
    bool visible = ImGui::Begin(imgui_id.c_str(), &open_state, flags);
    utl::scope_guard guard = [] { ImGui::End(); };
    ImGui::PopStyleVar();
    if (!open_state) panel.request_close();
    auto& state = update_and_get_runtime_state(panel);
    if (!visible) return {};
    if (!uses_toolbar) {
        vml::float2 cursor_pos = ImGui::GetCursorScreenPos();
        vml::float2 size = ImGui::GetContentRegionAvail();
        if (auto* view = panel.view()) view->display({cursor_pos, size});
    } else {
        Rect toolbar_frame = compute_toolbar_frame(state);
        float toolbar_height =
            toolbar_frame.size.y + ImGui::GetStyle().SeparatorSize;
        vml::float2 content_pos = {ImGui::GetWindowPos().x,
                                   toolbar_frame.origin.y + toolbar_height};
        vml::float2 size = ImGui::GetContentRegionAvail();
        size.y -= toolbar_height;
        if (auto* view = panel.view()) view->display({content_pos, size});
        if (auto* toolbar = panel.toolbar())
            display_toolbar(toolbar, toolbar_frame, state);
    }
    return {.focused = state.transient.focused};
}

// This must be called after ImGui::Begin()
RuntimePanelState& PanelRenderer::update_and_get_runtime_state(Panel& panel) {
    auto& state = runtime_state(panel);
    // This is the first point at which the ImGuiWindow belongs to us.
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    panel.m_geometry.frame.origin = imgui_window->Pos;
    panel.m_geometry.frame.size = imgui_window->Size;
    auto* viewport = imgui_window->Viewport;
    auto& transient = state.transient;
    transient.focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        !ImGui::FindBlockingModal(imgui_window);
    transient.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    transient.docked = imgui_window->DockIsActive;
    bool in_main_viewport = viewport == ImGui::GetMainViewport();
    transient.wants_native_toolbar =
        !in_main_viewport && imgui_window == imgui_window->Viewport->Window &&
        panel.toolbar() != nullptr;
    if (transient.wants_native_toolbar)
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

void PanelRenderer::end_frame() {
    auto& g = *GImGui;
    for (int i = 1; i < g.Viewports.size(); ++i) {
        auto* viewport = g.Viewports[i];
        auto* window = (Window*)viewport->PlatformHandle;
        if (!window) continue;
        bool wants_toolbar = m_viewports_with_native_toolbar.contains(viewport);
        window->set_style(make_window_style(wants_toolbar));
    }
}

RuntimePanelState& PanelRenderer::runtime_state(Panel const& panel) {
    auto itr = m_runtime.find(&panel);
    if (itr == m_runtime.end())
        itr = m_runtime.insert({&panel, RuntimePanelState{}}).first;
    return itr->second;
}