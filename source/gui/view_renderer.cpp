#define IMGUI_DEFINE_MATH_OPERATORS

#include "gui/view_renderer.h"

#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>

#include "application/debug_settings.h"
#include "application/window.h"
#include "application/window_desc.h"
#include "application/window_impl.h"
#include "gui/view.h"
#include "gui/view_manager.h"
#include "support/logger.h"

using namespace mosaic;
using namespace vml::short_types;

static void debug_bool(char const* name, bool value) {
    ImGui::Text("%-26s", name);
    ImGui::SameLine();
    ImGui::TextColored(value ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                             : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "%s", value ? "true" : "false");
};

static bool is_docked(ImGuiWindow const* window) {
    return window && window->DockIsActive;
}

static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

static void append_to_debugger(auto&& f) {
    if (ImGui::Begin("View Manager Debugger")) f();
    ImGui::End();
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
        // Correct the offset: use the docked window's position,
        // not the root dock host's position.
        g.ActiveIdClickOffset = g.IO.MouseClickedPos[0] - window->Pos;
    }
}

struct mosaic::RuntimeViewState {
    struct Frame {
        ImGuiWindow* imgui_window = nullptr;
        ImGuiViewport* viewport = nullptr;
        Window* window = nullptr;
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

ImGuiViewRenderer::ImGuiViewRenderer(ViewManager& manager)
    : m_manager(manager) {}

ImGuiViewRenderer::~ImGuiViewRenderer() = default;

RuntimeViewState& ImGuiViewRenderer::runtime_state(View const& view) {
    return m_runtime[&view];
}

bool ImGuiViewRenderer::is_main_viewport(ImGuiViewport const* viewport) const {
    return viewport == ImGui::GetMainViewport();
}

std::string ImGuiViewRenderer::make_imgui_id(View const& view) const {
    auto id = utl::strcat(view.title(), "###", view.type_name(), "_",
                          view.id().instance);
    return id;
}

void ImGuiViewRenderer::begin_frame() {
    for (auto& [view, state] : m_runtime) {
        bool docked_last_frame = state.frame.docked;
        state.frame = {};
        state.frame.docked_last_frame = docked_last_frame;
    }
    m_viewports_with_native_toolbar.clear();
}

#if 0
static void display_view_debug_info(View const& view,
                                    RuntimeViewState const& state) {
    if (!ImGui::TreeNode(&view, "%s", view.title().c_str())) {
return;
    }
    ImGui::PushID(&view);
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::Text("%s", view.title().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%p)", static_cast<void const*>(&view));
    ImGui::PopID();

    auto& [frame, persistent] = state;
    if (ImGui::CollapsingHeader("Frame Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Imgui Window: %p", (void*)frame.imgui_window);
        if (frame.imgui_window) {
            ImGui::SameLine();
            ImGui::Text("%s", frame.imgui_window->Name);
        }
        ImGui::Text("Viewport: %p", (void*)frame.viewport);
        ImGui::Text("Window: %p", (void*)frame.window);
        if (frame.window) {
            ImGui::SameLine();
            ImGui::Text("%s", frame.window->title().c_str());
        }
        ImGui::Separator();
        debug_bool("Focused", frame.focused);
        debug_bool("Hovered", frame.hovered);
        debug_bool("Docked", frame.docked);
        debug_bool("Wants native toolbar", frame.wants_native_toolbar);
        ImGui::Separator();
        debug_bool("Docked last frame", frame.docked_last_frame);
    }

    if (ImGui::CollapsingHeader("Persistent Data",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        debug_bool("Apply initial geometry", persistent.apply_initial_geometry);
        debug_bool("Window dragging", persistent.window_dragging);
    }

    if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::TreeNodeEx("Native Window",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
            if (auto* window = frame.window) {
                auto pos = window->position();
                ImGui::Text("Pos: (%d, %d)", pos.x, pos.y);
                ImGui::SameLine();
                auto size = window->size();
                ImGui::Text("Size: (%d, %d)", size.x, size.y);
            } else {
                ImGui::Text("NULL");
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("ImGui Window", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (auto* window = frame.imgui_window) {
                auto pos = (vml::int2)window->Pos;
                ImGui::Text("Pos: (%d, %d)", pos.x, pos.y);
                ImGui::SameLine();
                auto size = (vml::int2)window->Size;
                ImGui::Text("Size: (%d, %d)", size.x, size.y);
            } else {
                ImGui::Text("NULL");
            }
            ImGui::TreePop();
        }
    }
}
#endif

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

    bool open_state = true;
    bool visible =
        ImGui::Begin(make_imgui_id(view).c_str(), &open_state, flags);
    if (!open_state) view.request_close();

    auto& state = update_and_get_runtime_state(view);
    if (visible) {
        if (view.toolbar()) display_toolbar(view, state);
        view.display();
    }
    ImGui::End();

    append_to_debugger([&] {
        debug_bool("window dragging", state.persistent.window_dragging);
    });
}

void ImGuiViewRenderer::display_toolbar(View& view, RuntimeViewState& state) {
    assert(view.toolbar());
    float2 toolbar_padding = {7, 7};
    auto& toolbar = *view.m_toolbar;
    toolbar.set_padding(toolbar_padding);

    ImGuiWindow* imgui_window = state.frame.imgui_window;
    assert(imgui_window);

    float pos_x = 0.0f;
    if (state.frame.wants_native_toolbar && state.frame.window) {
        pos_x = compute_left_inset(*state.frame.window);
        float toolbar_height =
            state.frame.window->titlebar_height() - 2.0f * toolbar_padding.y;
        toolbar.set_height(toolbar_height);
    } else {
        toolbar.set_height(25.0f);
    }

    float pos_y = ImGui::GetCursorPosY();
    ImGui::SetCursorPosX(pos_x);
    vml::float2 toolbar_pos_screen =
        (float2)ImGui::GetWindowPos() + float2(pos_x, pos_y) - view.padding();

    ImGui::SetCursorScreenPos(toolbar_pos_screen);
    toolbar.display();
    ImGui::SetCursorPos({0, pos_y + toolbar.get_height() + 1});
    if (state.frame.docked || state.persistent.window_dragging) {
        float window_padding = ImGui::GetStyle().WindowPadding.x;
        vml::float2 size = {ImGui::GetWindowWidth() + 2 * window_padding,
                            toolbar.get_height()};
        ImRect toolbar_rect(toolbar_pos_screen, toolbar_pos_screen + size);
        dock_titlebar_behaviour(toolbar_rect, state.persistent.window_dragging);
    }
}

#if 0
void ImGuiViewRenderer::render_view(View& view) {
    auto layout = view.layout();
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    if (view.toolbar()) flags |= ImGuiWindowFlags_NoTitleBar;
    if (layout.maximized) {
#if 0
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoTitleBar;
        auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
#endif
    } else {
        ImGui::SetNextWindowSizeConstraints(layout.min_size,
                                            {FLT_MAX, FLT_MAX});
        auto& persistent = runtime_state(view).persistent;
        if (persistent.apply_initial_geometry) {
            ImGui::SetNextWindowPos(layout.position);
            ImGui::SetNextWindowSize(layout.size);
            persistent.apply_initial_geometry = false;
        }
    }
    // Padding/background/etc. can be configured here.
    bool open_state = true;
    bool open = ImGui::Begin(make_imgui_id(view).c_str(), &open_state, flags);
    // if (layout.maximized) ImGui::PopStyleVar();
    if (!open_state) view.request_close();
    auto state = update_and_get_runtime_state(view);
    if (open) {
        if (auto* toolbar = view.toolbar()) display_toolbar(view, state);
        auto cursor_pos_ss = (vml::float2)ImGui::GetWindowPos() +
                             (vml::float2)ImGui::GetCursorPos();
        view.display();
        if (want_debug_overlays())
            display_view_debug_info(state, cursor_pos_ss);
    }
    ImGui::End();
}

void ImGuiViewRenderer::display_toolbar(View& view, RuntimeViewState state) {
    assert(view.toolbar());
    // float2 padding = ImGui::GetStyle().WindowPadding;
    float2 padding = {7, 7};
    auto& toolbar = *view.m_toolbar;
    toolbar.set_padding(padding);
    ImGuiWindow* imgui_window = state.frame.imgui_window;
    assert(imgui_window);
    // A toolbar has two different jobs:
    // 1. In a secondary single-view platform window, it merges visually
    //    with the native titlebar.
    // 2. In the main viewport or a docked layout, it is simply an ImGui
    //    toolbar.
    // The platform-window style is handled separately by
    // update_platform_window_styles().
    float pos_x = 0.0f;
    if (state.frame.wants_native_toolbar && state.frame.window) {
        pos_x += compute_left_inset(*state.frame.window);
        float titlebar_height = state.frame.window->titlebar_height();
        toolbar.set_height(titlebar_height - 2.0f * padding.y);
    } else {
        toolbar.set_height(25.0f);
    }
    ImGui::SetCursorPosX(pos_x);
    float pos_y = ImGui::GetCursorPosY();
    // The old implementation manually manipulated DC.CursorPos.
    // Keep that behavior isolated to this renderer because it is an
    // ImGui implementation detail.
    imgui_window->DC.CursorPos =
        (float2)imgui_window->Pos + float2(pos_x, pos_y) - view.padding();
    toolbar.display();
    ImGui::SetCursorPos({0, pos_y + toolbar.get_height() + 1});
    // Only a docked toolbar needs explicit drag behavior.
    // A floating single-view window gets its native titlebar behavior
    // from the platform window itself.
    if (state.frame.docked) {
        float2 pos = ImGui::GetWindowPos();
        ImRect toolbar_rect(
            pos, pos + float2(ImGui::GetWindowWidth(), toolbar.get_height()));
        dock_titlebar_behaviour(imgui_window, toolbar_rect);
    }
}
#endif

// This must be called after ImGui::Begin()
RuntimeViewState&
ImGuiViewRenderer::update_and_get_runtime_state(View const& view) {
    auto& state = runtime_state(view);
    // This is the first point at which the ImGuiWindow belongs to us.
    auto* imgui_window = ImGui::GetCurrentWindowRead();
    auto* viewport = imgui_window->Viewport;
    auto& frame = state.frame;
    frame.imgui_window = imgui_window;
    frame.viewport = viewport;
    frame.window = static_cast<Window*>(viewport->PlatformHandle);
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

// bool ImGuiViewRenderer::is_single_view_in_platform_window(
//     View const& view, ImGuiViewport const* viewport) const {
//     /*
//      * Main viewport is explicitly excluded. It is always allowed to contain
//      * many application views and must never have its native Window style
//      * changed by a View.
//      */
//     if (!viewport || viewport == ImGui::GetMainViewport()) {
//         return false;
//     }
//     /*
//      * The authoritative answer is the set of Views we submitted into this
//      * viewport during this frame.
//      *
//      * This handles both:
//      *
//      *   - a floating single ImGui window
//      *   - a secondary viewport containing multiple docked/merged Views
//      *
//      * without relying on DockNode alone.
//      */
//     size_t count = 0;
//     for (auto const& [other, state] : m_runtime) {
//         if (state.frame.viewport != viewport) continue;
//         /*
//          * A hidden view doesn't submit a window and therefore isn't present
//          * here in normal operation.
//          */
//         if (!state.frame.imgui_window) continue;
//         ++count;
//         if (count > 1) return false;
//     }
//     if (count != 1) return false;
//     auto itr = m_runtime.find(&view);
//     return itr != m_runtime.end() && itr->second.frame.viewport == viewport;
// }

static void update_platform_window_style(Window& window, bool wants_toolbar) {
    if (wants_toolbar)
        window.set_style({
            .decoration = WindowDecoration::normal,
            .full_size_content = true,
            .large_titlebar = true,
        });
    else
        window.set_style({
            .decoration = WindowDecoration::none,
            .full_size_content = false,
            .large_titlebar = false,
        });
}

void ImGuiViewRenderer::update_platform_window_styles() {
    auto& g = *GImGui;
    for (int i = 1; i < g.Viewports.size(); ++i) {
        auto* viewport = g.Viewports[i];
        auto* window = (Window*)viewport->PlatformHandle;
        if (!window) continue;
        bool wants_toolbar = m_viewports_with_native_toolbar.contains(viewport);
        update_platform_window_style(*window, wants_toolbar);
    }
}