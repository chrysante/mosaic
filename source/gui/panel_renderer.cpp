#include "gui/panel_renderer.h"

#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/debug_settings.h"
#include "application/window.h"
#include "application/window_desc.h"
#include "gui/font.h"
#include "gui/panel.h"
#include "gui/toolbar.h"
#include "gui/view.h"
#include "imgui/imgui_impl_mosaic_macos.h"
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

static void dock_titlebar_behaviour(Rect titlebar_frame, bool& dragging) {
    ImGuiContext& g = *GImGui;
    auto* window = ImGui::GetCurrentWindow();
    if (window->Viewport != ImGui::GetMainViewport()) {
        dragging &= ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        return;
    }
    if (!dragging) {
        ImRect bb(titlebar_frame.min(), titlebar_frame.max());
        ImGuiID const id = window->GetID("##UndockDrag");
        if (!ImGui::ItemAdd(bb, id)) return;
        bool hovered = false;
        bool held = false;
        ImGui::ButtonBehavior(bb, id, &hovered, &held,
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
        Window* window;
        bool focused = false;
        bool hovered = false;
        bool docked = false;
        bool wants_native_titlebar = false;
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
    m_native_titlebar_size_map.clear();
}

static std::string make_imgui_id(Panel const& view) {
    return utl::strcat(view.title(), "##", (void const*)&view);
}

static WindowTitlebarSize to_titlebar_size(PanelToolbarHeight toolbar_height,
                                           bool have_toolbar) {
    if (!have_toolbar) return WindowTitlebarSize::small;
    switch (toolbar_height) {
    case PanelToolbarHeight::small:
        return WindowTitlebarSize::small;
    case PanelToolbarHeight::medium:
        return WindowTitlebarSize::medium;
    case PanelToolbarHeight::large:
        return WindowTitlebarSize::large;
    }
}

namespace {
struct TitlebarFrame {
    Rect outer;
    Rect inner;
};
} // namespace

static TitlebarFrame compute_titlebar_frame(Panel const& panel,
                                            RuntimePanelState state) {
    WindowTitlebarSize titlebar_size = to_titlebar_size(
        panel.style().toolbar_height, panel.toolbar() != nullptr);
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* window = get_window_from_viewport(imgui_window->Viewport);
    float width = imgui_window->Size.x;
    float offset_y = ImGui::GetCursorPosY() - imgui_window->WindowPadding.y;
    if (!window || !state.transient.wants_native_titlebar) {
        Rect outer = {{imgui_window->Pos.x, imgui_window->Pos.y + offset_y},
                      {width, window_titlebar_size(titlebar_size)}};
        return {outer, outer};
    }
    auto height = window->titlebar_height();
    Rect outer = {
        {imgui_window->Pos.x, imgui_window->Pos.y + offset_y},
        {width, height},
    };
    float left_inset = compute_left_inset(*window);
    Rect inner = {{outer.origin.x + left_inset, outer.origin.y},
                  {outer.size.x - left_inset, outer.size.y}};
    return {outer, inner};
}

static void draw_window_title(std::string_view title, Rect frame) {
    char const* begin = title.data();
    char const* end = title.data() + title.size();
    ui::push_font({.weight_override = FontWeight::medium});
    vml::float2 title_size = ImGui::CalcTextSize(begin, end, true);
    vml::float2 position = frame.origin + 0.5f * (frame.size - title_size);
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), begin, end);
    ui::pop_font();
}

static float title_padding(PanelToolbarHeight height) {
    switch (height) {
    case PanelToolbarHeight::small:
        return 3.0f;
    case PanelToolbarHeight::medium:
        return 6.0f;
    case PanelToolbarHeight::large:
        return 10.0f;
    }
}

static void display_titlebar(Panel& panel, TitlebarFrame frame,
                             RuntimePanelState state) {
    if (auto* toolbar = panel.toolbar()) {
        float padding = title_padding(panel.style().toolbar_height);
        ui::internal::set_toolbar_padding(toolbar,
                                          {padding, padding, padding, padding});
        toolbar->display(frame.inner);
    } else {
        draw_window_title(panel.title(), frame.outer);
    }
    float separator_size = ImGui::GetStyle().SeparatorSize;
    vml::float2 line_min = {ImGui::GetWindowPos().x,
                            frame.outer.origin.y + frame.outer.size.y};
    vml::float2 line_max =
        line_min + vml::float2{ImGui::GetWindowSize().x, separator_size};
    auto* DL = ImGui::GetWindowDrawList();
    DL->AddRectFilled(line_min, line_max,
                      ImGui::GetColorU32(ImGuiCol_Separator));
    if (state.transient.docked || state.persistent.window_dragging)
        dock_titlebar_behaviour(frame.inner, state.persistent.window_dragging);
}

static ImGuiWindowClass window_class = [] {
    ImGuiWindowClass window_class;
    window_class.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoDecoration;
    return window_class;
}();

#ifndef NDEBUG
static void draw_debug_overlay(vml::float2 position,
                               PanelGeometry const& geometry,
                               Window const* window) {
    vml::float2 window_pos =
        window ? (vml::float2)window->position() : vml::float2(-1.0f, -1.0f);
    char buffer[256];
    std::snprintf(
        buffer, sizeof buffer,
        "Position: %.1f, %.1f\nSize: %.1f, %.1f\nWindowPos: %.1f, %.1f",
        geometry.frame.origin.x, geometry.frame.origin.y, geometry.frame.size.x,
        geometry.frame.size.y, window_pos.x, window_pos.y);
    ImGui::GetWindowDrawList()->AddText(position, IM_COL32(255, 0, 255, 255),
                                        buffer);
}
#endif // NDEBUG

PanelRenderer::Result PanelRenderer::render_panel(Panel& panel) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoTitleBar;
    if (auto const& state = runtime_state(panel);
        !state.transient.docked_last_frame && !state.persistent.window_dragging)
        flags |= ImGuiWindowFlags_NoMove;
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
    if (!visible) return {};

    auto& state = update_and_get_runtime_state(panel);
    bool const draw_titlebar =
        state.transient.docked ? panel.toolbar() != nullptr : true;
    vml::float2 content_pos = {0.0f, 0.0f};
    if (!draw_titlebar) {
        content_pos = ImGui::GetCursorScreenPos();
        vml::float2 size = ImGui::GetContentRegionAvail();
        if (auto* view = panel.view()) view->display({content_pos, size});
    } else {
        TitlebarFrame titlebar_frame = compute_titlebar_frame(panel, state);
        float title_title_height =
            titlebar_frame.outer.size.y + ImGui::GetStyle().SeparatorSize;
        content_pos = {ImGui::GetWindowPos().x,
                       titlebar_frame.outer.origin.y + title_title_height};
        vml::float2 size = ImGui::GetContentRegionAvail();
        size.y -= title_title_height;
        if (auto* view = panel.view()) view->display({content_pos, size});
        display_titlebar(panel, titlebar_frame, state);
    }
    if (want_debug_overlays())
        draw_debug_overlay(content_pos, panel.geometry(),
                           state.transient.window);
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
    transient.window = imgui_window->ViewportOwned
                           ? (Window*)imgui_window->Viewport->PlatformHandle
                           : nullptr;
    transient.focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        !ImGui::FindBlockingModal(imgui_window);
    transient.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    transient.docked = imgui_window->DockIsActive;
    bool in_main_viewport = viewport == ImGui::GetMainViewport();
    transient.wants_native_titlebar =
        !in_main_viewport && imgui_window == imgui_window->Viewport->Window;
    WindowTitlebarSize titlebar_size = to_titlebar_size(
        panel.style().toolbar_height, panel.toolbar() != nullptr);
    if (transient.wants_native_titlebar)
        m_native_titlebar_size_map.emplace(viewport, titlebar_size);
    return state;
}

static WindowStyle make_window_style(ImGuiViewport const* viewport,
                                     WindowTitlebarSize titlebar_size) {
    WindowStyle style = mosaic::make_window_style(viewport);
    style.titlebar_size = titlebar_size;
    return style;
}

void PanelRenderer::end_frame() {
    auto& g = *GImGui;
    for (int i = 1; i < g.Viewports.size(); ++i) {
        auto* viewport = g.Viewports[i];
        auto* window = (Window*)viewport->PlatformHandle;
        if (!window) continue;
        auto itr = m_native_titlebar_size_map.find(viewport);
        WindowTitlebarSize titlebar_size =
            itr != m_native_titlebar_size_map.end() ? itr->second
                                                    : WindowTitlebarSize::small;
        window->set_style(make_window_style(viewport, titlebar_size));
    }
}

RuntimePanelState& PanelRenderer::runtime_state(Panel const& panel) {
    auto itr = m_runtime.find(&panel);
    if (itr == m_runtime.end())
        itr = m_runtime.insert({&panel, RuntimePanelState{}}).first;
    return itr->second;
}