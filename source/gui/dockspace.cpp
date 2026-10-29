#include "gui/dockspace.h"

#include <algorithm>
#include <array>
#include <exception>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/function_view.hpp>
#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include "application/debug_settings.h"
#include "application/window.h"
#include "gui/toolbar2.h"
#include "gui/view.h"

using namespace mosaic;
using namespace vml::short_types;

using internal::DockspaceImpl;

struct internal::DockspaceImpl {
    vml::float2 min_window_size = {250, 100};
    std::array<Owner<ui::View>, 3> toolbars;
    ImGuiWindow* main_imgui_window = nullptr;
    size_t spacing_count = 0;
    unsigned main_dock_id = 0;
    bool has_menubar = false;
};

static constexpr auto main_window_id = "MainWindow";
static constexpr auto main_dockspace_id = "MainWindow_Dockspace";

static void with_window_size_constraints(vml::float2 min_size, auto&& block) {
    ImGuiStyle& style = ImGui::GetStyle();
    utl::scope_guard restore = [&, old_size = style.WindowMinSize] {
        style.WindowMinSize = old_size;
    };
    style.WindowMinSize = min_size;
    block();
}

Dockspace::Dockspace()
    : m_impl(std::make_unique<DockspaceImpl>()) {}

Dockspace::~Dockspace() = default;

static void main_window(DockspaceImpl& impl, float toolbar_height,
                        utl::function_view<void()> menubar_content);
static void display_toolbars(DockspaceImpl& impl, Window& window);

void Dockspace::display(Window* window,
                        utl::function_view<void()> menubar_content) {
    if (!window) return;
    window->set_style({.full_size_content = true, .large_titlebar = true});
    main_window(*m_impl, window->titlebar_height(), menubar_content);
    display_toolbars(*m_impl, *window);
}

void Dockspace::set_left_toolbar(Owner<ui::View> toolbar) {
    m_impl->toolbars[0] = std::move(toolbar);
}

void Dockspace::set_center_toolbar(Owner<ui::View> toolbar) {
    m_impl->toolbars[1] = std::move(toolbar);
}

void Dockspace::set_right_toolbar(Owner<ui::View> toolbar) {
    m_impl->toolbars[2] = std::move(toolbar);
}

void Dockspace::take_focus() {
    if (!m_impl->main_imgui_window) return;
    auto flags = ImGuiFocusRequestFlags_RestoreFocusedChild |
                 ImGuiFocusRequestFlags_UnlessBelowModal;
    ImGui::FocusWindow(m_impl->main_imgui_window, flags);
}

bool Dockspace::has_menubar() const { return m_impl->has_menubar; }

void Dockspace::toggle_menubar() { m_impl->has_menubar ^= true; }

static ImGuiID submit_dockspace() {
    ImGuiDockNodeFlags flags = 0;
    flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
    flags |= ImGuiDockNodeFlags_NoCloseButton;
    flags |= ImGuiDockNodeFlags_AutoHideTabBar;
    ImGuiID id = ImGui::GetID(main_dockspace_id);
    ImGui::DockSpace(id, /* size = */ {}, flags);
    return id;
}

static void main_window(DockspaceImpl& impl, float toolbar_height,
                        utl::function_view<void()> menubar_content) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float2 local_pos = float2(0, toolbar_height + 1);
    auto window_position = (float2)viewport->Pos + local_pos;
    auto window_size = (float2)viewport->Size - local_pos;
    ImGui::SetNextWindowPos(window_position);
    ImGui::SetNextWindowSize(window_size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags windowFlags = 0;
    windowFlags |= ImGuiWindowFlags_NoTitleBar;
    windowFlags |= ImGuiWindowFlags_NoResize;
    windowFlags |= ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoNavFocus;
    if (impl.has_menubar) windowFlags |= ImGuiWindowFlags_MenuBar;
    auto& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        float2(1, 1) * (float2)style.FramePadding);
    ImGui::Begin(main_window_id, nullptr, windowFlags);
    ImGui::PopStyleVar(6);
    if (impl.has_menubar && ImGui::BeginMenuBar()) {
        if (menubar_content) menubar_content();
        ImGui::EndMenuBar();
    }
    with_window_size_constraints(
        impl.min_window_size, [&] { impl.main_dock_id = submit_dockspace(); });
    impl.main_imgui_window = ImGui::GetCurrentWindowRead();
    ImGui::End();
}

/// Calls `ImGui::Begin` with every possible interaction and decoration disabled
[[nodiscard]] static bool begin_toolbar_window(char const* id, float2 position,
                                               float2 size,
                                               ImGuiWindowFlags flags = 0) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);
    flags |= ImGuiWindowFlags_NoDocking;
    flags |= ImGuiWindowFlags_NoTitleBar;
    flags |= ImGuiWindowFlags_NoResize;
    flags |= ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoNavFocus;
    flags |= ImGuiWindowFlags_NoFocusOnAppearing;
    flags |= ImGuiWindowFlags_NoScrollbar;
    flags |= ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    bool result = ImGui::Begin(id, NULL, flags);
    ImGui::PopStyleVar(2);
    return result;
}

/// Draws a window in the toolbar area that renders the background color
static void toolbar_bg_window(float2 position, float2 size) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open =
        begin_toolbar_window("Main_Toolbar_BG", position, size + float2(0, 1),
                             ImGuiWindowFlags_NoInputs);
    ImGui::PopStyleVar();
    if (!open) return;
    auto* imgui_window = ImGui::GetCurrentWindow();
    auto* DL = imgui_window->DrawList;
    // Background
    auto bg_col = ImGui::ColorConvertFloat4ToU32(
        ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    DL->AddRectFilled(position, position + (float2)imgui_window->Size, bg_col);
    // Bottom separator
    auto separator_col = ImGui::ColorConvertFloat4ToU32(
        GImGui->Style.Colors[ImGuiCol_Separator]);
    float2 line_pos = position + float2(0, imgui_window->Size.y - 1);
    DL->AddLine(line_pos, line_pos + float2(imgui_window->Size.x, 0),
                separator_col);
    ImGui::End();
}

static void erase_duplicates(auto& vector) {
    auto last = std::unique(vector.begin(), vector.end());
    vector.resize(last - vector.begin());
}

static void erase_if(auto& vector, auto pred) {
    auto itr = std::remove_if(vector.begin(), vector.end(), pred);
    vector.erase(itr, vector.end());
}

// Returns the desired toolbar-split-x-coordinates in screen-space
static utl::small_vector<float, 2> find_toolbar_splits(Window& window,
                                                       unsigned main_dock_id) {
    struct Rect {
        int min;
        int max;
    };
    int window_pos_x = ImGui::GetMainViewport()->Pos.x;
    auto potential_rects = [&] {
        ImGuiDockNode const* main_node =
            ImGui::DockBuilderGetNode(main_dock_id);
        assert(main_node);
        utl::stack<ImGuiDockNode const*> stack;
        stack.push(main_node);
        utl::small_vector<Rect, 8> result;
        while (!stack.empty()) {
            auto* node = stack.pop();
            if (!node->IsLeafNode()) {
                stack.push(node->ChildNodes[0]);
                stack.push(node->ChildNodes[1]);
                continue;
            }
            if (node == main_node) continue;
            if (!node->IsVisible) continue;
            if ((int)node->Pos.y != (int)main_node->Pos.y) continue;
            if ((int)node->Size.x >= (int)main_node->Size.x) continue;
            int min = (int)node->Pos.x - window_pos_x;
            int max = min + (int)node->Size.x;
            result.push_back({min, max});
        }
        return result;
    }();
    utl::small_vector<int> positions;
    for (auto rect : potential_rects) {
        positions.push_back(rect.min);
        positions.push_back(rect.max);
    }
    std::sort(positions.begin(), positions.end());
    erase_duplicates(positions);
    erase_if(positions, [&](int p) { return p <= 0 || p >= window.size().x; });
    int const diff = ImGui::GetStyle().DockingSeparatorSize;
    // Merge adjacent dock-node boundaries that are separated only by the
    // docking separator. After merging, advance to the next pair: the remaining
    // right edge belongs to the same dock node, so rechecking it would compare
    // the wrong edges.
    for (std::size_t i = 1; i < positions.size(); ++i) {
        int& a = positions[i - 1];
        int b = positions[i];
        assert(b >= a && "positions must be sorted here");
        if ((b - a) <= diff) {
            a = std::midpoint(a, b);
            positions.erase(positions.begin() + i);
        }
    }
    switch (positions.size()) {
    case 0:
        return {};
    case 1:
        return {(float)(positions.front() + window_pos_x)};
    default:
        return {(float)(positions.front() + window_pos_x),
                (float)(positions.back() + window_pos_x)};
    }
}

static void draw_toolbars(ui::Rect frame,
                          std::span<ui::View* const, 3> toolbars,
                          std::span<float const> widths) {
    vml::float2 pos_offset1 = {widths[0], 0.0f};
    vml::float2 pos_offset2 = {widths[0] + widths[1], 0.0f};

    ui::Rect toolbar_frames[3] = {
        {frame.origin, {widths[0], frame.size.y}},
        {frame.origin + pos_offset1, {widths[1], frame.size.y}},
        {frame.origin + pos_offset2, {widths[2], frame.size.y}}};

    for (std::size_t i = 0; i < 3; ++i)
        toolbars[i]->display(toolbar_frames[i]);
}

static void toolbar_layout_one(ui::Rect frame,
                               std::span<ui::View* const, 3> toolbars,
                               std::span<float const> splits) {
    assert(splits.empty());
    float t0_width = toolbars[0]->calc_intrinsic_size({-1.0f, frame.size.y}).x;
    float t2_width = toolbars[2]->calc_intrinsic_size({-1.0f, frame.size.y}).x;
    float t1_width = frame.size.x - t0_width - t2_width;
    draw_toolbars(frame, toolbars, {{t0_width, t1_width, t2_width}});
}

static void toolbar_layout_two(ui::Rect frame,
                               std::span<ui::View* const, 3> toolbars,
                               std::span<float const> splits) {
    assert(splits.size() == 1);

    float split = splits.front();
    if (split >= 0.5f * frame.size.x) {
        // Left section is big
        float t0_width =
            toolbars[0]->calc_intrinsic_size({-1.0f, frame.size.y}).x;
        float t1_width = split - frame.origin.x - t0_width;
        float t2_width = frame.size.x - t0_width - t1_width;
        draw_toolbars(frame, toolbars, {{t0_width, t1_width, t2_width}});
    } else {
        // Right section is big
        float t0_width = split - frame.origin.x;
        float t2_width =
            toolbars[2]->calc_intrinsic_size({-1.0f, frame.size.y}).x;
        float t1_width = frame.size.x - t0_width - t2_width;
        draw_toolbars(frame, toolbars, {{t0_width, t1_width, t2_width}});
    }
}

static void toolbar_layout_three(ui::Rect frame,
                                 std::span<ui::View* const, 3> toolbars,
                                 std::span<float const> splits) {
    assert(splits.size() == 2);
    std::array<float, 3> widths = {splits[0] - frame.origin.x,
                                   splits[1] - splits[0],
                                   frame.max().x - splits[1]};
    draw_toolbars(frame, toolbars, widths);
}

static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

static std::array<ui::internal::ToolbarPadding2, 3>
dockspace_resolve_toolbar_padding() {
    using Padding = ui::internal::ToolbarPadding2;

    vml::float2 padding_value = {6.0f, 6.0f};
    float item_spacing = ImGui::GetStyle().ItemSpacing.x;
    Padding padding = {padding_value.x, padding_value.x, padding_value.y,
                       padding_value.y};
    std::array<Padding, 3> result = {padding, padding, padding};
    result[0].right = item_spacing / 2;
    result[1].left = item_spacing / 2;
    result[1].right = item_spacing / 2;
    result[2].left = item_spacing / 2;
    return result;
}

/// Displays the window in which the toolbar is drawn through \p content
static void toolbar_window(float2 position, float2 size,
                           std::span<float const> splits,
                           std::array<float, 2> insets,
                           std::span<ui::View* const, 3> toolbars) {
    toolbar_bg_window(position, size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    bool open = begin_toolbar_window("Main_Toolbar", position, size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open) return;

    ui::Rect frame = {{position.x + insets[0], position.y},
                      {size.x - insets[0] - insets[1], size.y}};

    // Window content
    auto padding = dockspace_resolve_toolbar_padding();
    for (std::size_t i = 0; i < 3; ++i)
        ui::internal::set_toolbar_padding(toolbars[i], padding[i]);
    switch (splits.size()) {
    case 0:
        toolbar_layout_one(frame, toolbars, splits);
        break;
    case 1:
        toolbar_layout_two(frame, toolbars, splits);
        break;
    case 2:
        toolbar_layout_three(frame, toolbars, splits);
        break;
    default:
        assert(false);
        std::terminate();
    }
    if (ImGui::IsWindowFocused() && GImGui->WindowsFocusOrder.size() > 1 &&
        ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left]) {
        ImGui::FocusWindow(*(GImGui->WindowsFocusOrder.end() - 2));
    }
    if (want_debug_overlays()) {
        auto* DL = ImGui::GetWindowDrawList();
        for (float split : splits)
            DL->AddLineV(split, position.y, position.y + size.y,
                         IM_COL32(255, 0, 255, 255), 3.0f);
    }
    ImGui::End();
}

static void display_toolbars(DockspaceImpl& impl, Window& window) {
    float height = window.titlebar_height();
    auto splits = find_toolbar_splits(window, impl.main_dock_id);
    assert(splits.size() <= 2);
    float width = window.size().x;
    vml::float2 viewport_pos = ImGui::GetMainViewport()->Pos;
    erase_if(splits, [&](float pos) {
        return (pos - viewport_pos.x) >= width * 0.33 &&
               (pos - viewport_pos.x) <= width * 0.67;
    });
    std::array<ui::View*, 3> toolbars = {
        impl.toolbars[0].get(), impl.toolbars[1].get(), impl.toolbars[2].get()};
    toolbar_window(viewport_pos, {width, height}, splits,
                   {compute_left_inset(window), 0.0f}, toolbars);
}
