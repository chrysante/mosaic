#include "gui/view.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>
#include <utl/strcat.hpp>

#include "application/input_event.h"
#include "application/window.h"
#include "gui/view_registry.h"
#include "service/service_context.h"
#include "support/logger.h"
#include "support/serialization.h" // IWYU pragma: keep

using namespace mosaic;
using namespace vml::short_types;

namespace {

class IDFactory {
public:
    int make_unique(int existing) {
        if (existing >= 0 && used_ids.insert(existing).second) return existing;
        while (used_ids.contains(id_counter))
            ++id_counter;
        used_ids.insert(id_counter);
        return id_counter;
    }

private:
    int id_counter;
    utl::hashset<int> used_ids;
};

static utl::hashmap<std::string, IDFactory> g_id_factory_map;

} // namespace

View::View(ViewDescription data)
    : m_data{.size = data.default_size,
             .min_size = data.min_size,
             .padding = {std::nullopt, std::nullopt}} {}

bool View::focused() const { return m_data.is_focused; }

vml::float2 View::padding() const {
    auto& style = ImGui::GetStyle();
    return {
        m_data.padding.x.value_or(style.WindowPadding.x),
        m_data.padding.y.value_or(style.WindowPadding.y),
    };
}

Window& View::window() const {
    assert(
        m_data.window &&
        "Cannot call this method during init(). This should change in the future");
    return *m_data.window;
}

float2 View::screen_space_to_view_space(float2 position) const {
    return position - m_data.position;
}

float2 View::view_space_to_screen_space(float2 position) const {
    return position + m_data.position;
}

void View::set_focused() {
    ImGui::FocusWindow((ImGuiWindow*)m_data.imgui_window);
    if (m_data.window) m_data.window->set_focused();
}

void View::maximize() {
    m_data.maximized = true;
    // TODO: Send message to window
}

void View::restore() {
    m_data.maximized = false;
    // TODO: Send message to window
}

void View::toggle_maximize() {
    if (maximized()) restore();
    else maximize();
}

void View::do_init() {
    m_data.ID = g_id_factory_map[m_data.static_name].make_unique(m_data.ID);
    if (m_data.title.empty()) m_data.title = m_data.static_name;
    init();
}

static bool is_docked(ImGuiWindow* window) {
    return window && window->DockIsActive;
}

void View::do_frame() {
    auto* imgui_window = (ImGuiWindow*)m_data.imgui_window;
    ImGuiWindowFlags flags = m_data.next_frame_flags;
    flags |= ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_NoResize;
    if (m_toolbar) flags |= ImGuiWindowFlags_NoTitleBar;
    /// If we are not docked we don't allow movement because otherwise we would
    /// be able to move windows by dragging anywhere in its area. We allow
    /// dragging separately in the title bar area
    if (!is_docked(imgui_window) && imgui_window && !imgui_window->ParentWindow)
        flags |= ImGuiWindowFlags_NoMove;
#if 0
    if (toolbar()) flags |= ImGuiWindowFlags_NoDockingTabIndicator;
#endif
    // Begin block
    if (!imgui_window && !m_data.maximized && !m_data.docked) {
        ImGui::SetNextWindowPos(m_data.position);
        ImGui::SetNextWindowSize(m_data.size);
    }
    if (m_data.maximized) {
        auto* const viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowPos({0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        flags |= ImGuiWindowFlags_NoResize;
        flags |= ImGuiWindowFlags_NoMove;
        flags |= ImGuiWindowFlags_NoTitleBar;
    } else {
        ImGui::SetNextWindowSizeConstraints(m_data.min_size,
                                            {FLT_MAX, FLT_MAX});
    }
    auto& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        m_data.maximized ? 0 : style.WindowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding());
    if (m_data.bg_color)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, *m_data.bg_color);
    m_data.title_id = utl::strcat(title(), "###", static_name());
    if (m_data.maximized) m_data.title_id += "_FS";
    if (m_data.ID != 0) m_data.title_id += utl::strcat("_", m_data.ID);
    bool* begin_open_flag = is_docked(imgui_window) ? &m_data.open : nullptr;
    bool open = ImGui::Begin(m_data.title_id.c_str(), begin_open_flag, flags);
    m_data.next_frame_flags = 0;
    if (m_data.bg_color) ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (m_data.maximized) ImGui::PopStyleVar();
    imgui_window = ImGui::GetCurrentWindow();
#if 0
    imgui_window->UserPointer = this;
#endif
    m_data.imgui_window = imgui_window;
    m_data.is_focused =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        !ImGui::FindBlockingModal(imgui_window);
    if (open) {
        m_data.position = imgui_window->WorkRect.Min;
        m_data.size = imgui_window->Size;
        m_data.docked = is_docked(imgui_window);
        auto* viewport = (ImGuiViewportP*)imgui_window->Viewport;
        m_data.window = (Window*)viewport->PlatformHandle;
        if (m_data.window && GImGui->CurrentViewport->Window == imgui_window) {
            bool have_toolbar = m_toolbar.has_value();
            m_data.window->set_style({.full_size_content = have_toolbar,
                                      .large_titlebar = have_toolbar});
        }
        if (m_data.window) {
            display_toolbar();
            display();
        }
        m_input_state.hovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    }
    ImGui::End();
}

/// Copied from Dockspace.cpp
static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

static void dock_titlebar_behaviour(ImGuiWindow* im_window, ImRect toolbar_bb) {
    auto& g = *GImGui;
    bool window_dragging =
        ImGui::IsWindowFocused() &&
        toolbar_bb.Contains(g.IO.MouseClickedPos[ImGuiMouseButton_Left]) &&
        !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left, 20);
    if (window_dragging) {
        auto* node = im_window->DockNode;
        ImGui::StartMouseMovingWindowOrNode(
            im_window, node->Windows.Size == 1 ? node : nullptr, true);
    }
}

void View::display_toolbar() {
    if (!m_toolbar) return;
    float2 padding = {7, 7};
    m_toolbar->set_padding(padding);
    float posx = 0;
    ImGuiWindow* im_window = (ImGuiWindow*)m_data.imgui_window;
    assert(im_window && "This should not be called if we don't have a window");
    /// We treat the case where we are not docked but don't have a window as if
    /// we are docked, but that should not happen anyways. We could probably
    /// assert that.
    bool docked = m_data.docked || !m_data.window;
    if (docked) {
        m_toolbar->set_height(25);
    } else {
        posx += compute_left_inset(window());
        float titlebar_height = window().titlebar_height();
        m_toolbar->set_height(titlebar_height - 2 * padding.y);
        internal::set_window_titlebar_controls_hovered(
            window(), !ImGui::IsAnyItemHovered());
    }
    ImGui::SetCursorPosX(posx);
    float posy = ImGui::GetCursorPosY();
    im_window->DC.CursorPos =
        (float2)im_window->Pos + float2(posx, posy) - this->padding();
    m_toolbar->display();
    ImGui::SetCursorPos({0, posy + m_toolbar->get_height() + 1});
    if (docked) {
        float2 pos = ImGui::GetWindowPos();
        ImRect toolbar_rect(pos, pos + float2(ImGui::GetWindowWidth(),
                                              m_toolbar->get_height()));
        dock_titlebar_behaviour(im_window, toolbar_rect);
    }
}

void View::do_on_input(InputEvent& event) {
    using enum InputEventMask;
    event.handle<mouse_down>([&](MouseEvent const&) {
        m_input_state.capture = m_input_state.hovered;
    });
    event.handle<mouse_up>(
        [&](MouseEvent const&) { m_input_state.capture = false; });
    if (test(event.type() & (mouse_dragged | scroll_wheel | magnify)) &&
        !(m_input_state.hovered || m_input_state.capture))
        return;
    if (test(event.type() & key) && !focused()) return;
    m_command_palette.on_input(event);
}

YAML::Node View::do_serialize() const {
    YAML::Node node;
    node["ID"] = m_data.ID;
    node["Position"] = m_data.position;
    node["Size"] = m_data.size;
    node["Docked"] = m_data.docked;
    node["Hidden"] = m_data.hidden;
    node["Static Name"] = m_data.static_name;
    node["Derived Data"] = serialize();
    return node;
}

std::unique_ptr<View> View::do_deserialize(YAML::Node const& node,
                                           ServiceContext& service_context) {
    try {
        int ID = node["ID"].as<int>();
        std::string static_name = node["Static Name"].as<std::string>();
        if (ID < 0) throw std::runtime_error("Invalid View ID");
        auto entry = ViewRegistry::get(static_name);
        if (!entry) {
            Logger::error("Failed to deserialize view: unknown static name: ",
                          static_name);
            return nullptr;
        }
        auto view = entry->create_view(service_context);
        view->m_data.ID = ID;
        view->m_data.position = node["Position"].as<float2>(float2{300, 300});
        view->m_data.size = node["Size"].as<float2>(float2(float2{500, 500}));
        view->m_data.hidden = node["Hidden"].as<bool>(false);
        view->m_data.docked = node["Docked"].as<bool>(false);
        view->deserialize(node["Derived Data"]);
        return view;
    } catch (std::exception const& e) {
        Logger::error(e.what());
    }
    return nullptr;
}