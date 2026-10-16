#include "gui/dockspace.h"

#include <algorithm>
#include <exception>
#include <numeric>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/function_view.hpp>
#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include "application/window.h"
#include "gui/toolbar.h"
#include "gui/toolbar_renderer.h"

using namespace mosaic;
using namespace vml::short_types;

using internal::DockspaceImpl;

struct internal::DockspaceImpl {
    DockspaceImpl() {
        for (std::size_t i = 0; i < 3; ++i)
            toolbar_renderers[i].set_toolbar(&toolbars[i]);
    }

    vml::float2 min_window_size = {250, 100};
    std::array<Toolbar, 3> toolbars;
    std::array<ToolbarRenderer, 3> toolbar_renderers;
    ImGuiWindow* main_imgui_window = nullptr;
    size_t spacing_count = 0;
    unsigned main_dock_id = 0;
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

static void main_window(DockspaceImpl& impl, float toolbar_height);
static void display_toolbars(DockspaceImpl& impl, Window& window);

void Dockspace::display(Window* window) {
    if (!window) return;
    window->set_style({.full_size_content = true, .large_titlebar = true});
    main_window(*m_impl, window->titlebar_height());
    display_toolbars(*m_impl, *window);
}

ToolbarStyle const toolbar_style{.padding = {6.0f, 6.0f},
                                 .flags = ToolbarFlags::no_decoration};

void Dockspace::set_left_toolbar(Toolbar toolbar) {
    m_impl->toolbars[0] = std::move(toolbar);
}

void Dockspace::set_center_toolbar(Toolbar toolbar) {
    m_impl->toolbars[1] = std::move(toolbar);
}

void Dockspace::set_right_toolbar(Toolbar toolbar) {
    m_impl->toolbars[2] = std::move(toolbar);
}

void Dockspace::take_focus() {
    if (!m_impl->main_imgui_window) return;
    auto flags = ImGuiFocusRequestFlags_RestoreFocusedChild |
                 ImGuiFocusRequestFlags_UnlessBelowModal;
    ImGui::FocusWindow(m_impl->main_imgui_window, flags);
}

static ImGuiID submit_dockspace() {
    ImGuiDockNodeFlags flags = 0;
    flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
    flags |= ImGuiDockNodeFlags_NoCloseButton;
    flags |= ImGuiDockNodeFlags_AutoHideTabBar;
    ImGuiID id = ImGui::GetID(main_dockspace_id);
    ImGui::DockSpace(id, /* size = */ {}, flags);
    return id;
}

static void main_window(DockspaceImpl& impl, float toolbar_height) {
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
    auto& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        float2(1, 1) * (float2)style.FramePadding);
    ImGui::Begin(main_window_id, nullptr, windowFlags);
    with_window_size_constraints(
        impl.min_window_size, [&] { impl.main_dock_id = submit_dockspace(); });
    impl.main_imgui_window = ImGui::GetCurrentWindowRead();
    ImGui::End();
    ImGui::PopStyleVar(6);
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

static utl::small_vector<int, 2> find_toolbar_splits(Window& window,
                                                     unsigned main_dock_id) {
    struct Rect {
        int min;
        int max;
    };
    auto potential_rects = [&] {
        int window_pos_x = ImGui::GetMainViewport()->Pos.x;
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
        return {positions.front()};
    default:
        return {positions.front(), positions.back()};
    }
}

namespace {

struct ToolbarLayout {
    float height;
    float total_width;
    float left_inset;
};

} // namespace

static void toolbar_layout_one(ToolbarLayout layout, ToolbarStyle const& style,
                               std::span<ToolbarRenderer> renderers,
                               std::span<ToolbarPadding const> padding,
                               std::span<int const> /* splits */) {
    float t0_width = renderers[0].get_width_without_spacers(style, padding[0]);
    float t2_width = renderers[2].get_width_without_spacers(style, padding[2]);
    renderers[0].display(style, {.position = {layout.left_inset, 0.0f},
                                 .size = {t0_width, layout.height},
                                 .padding = padding[0]});
    float t1_width =
        layout.total_width - layout.left_inset - t0_width - t2_width;
    renderers[1].display(style,
                         {.position = {layout.left_inset + t0_width, 0.0f},
                          .size = {t1_width, layout.height},
                          .padding = padding[1]});
    renderers[2].display(style,
                         {.position = {layout.total_width - t2_width, 0.0f},
                          .size = {t2_width, layout.height},
                          .padding = padding[2]});
}

static void toolbar_layout_two(ToolbarLayout layout, ToolbarStyle const& style,
                               std::span<ToolbarRenderer> renderers,
                               std::span<ToolbarPadding const> padding,
                               std::span<int const> splits) {
    float spacer = splits[0];
    float position[2] = {0, spacer};
    float width[2] = {spacer, layout.total_width - spacer};
    if (spacer >= layout.total_width / 2) {
        // Left section is big
        // 1. section
        float t0_width =
            renderers[0].get_width_without_spacers(style, padding[0]);
        renderers[0].display(
            style, {.position = {position[0] + layout.left_inset, 0.0f},
                    .size = {t0_width, layout.height},
                    .padding = padding[0]});
        renderers[1].display(
            style,
            {.position = {position[0] + layout.left_inset + t0_width, 0.0f},
             .size = {width[0] - t0_width - layout.left_inset, layout.height},
             .padding = padding[1]});
        // 2. section
        renderers[2].display(style, {.position = {position[1], 0.0f},
                                     .size = {width[1], layout.height},
                                     .padding = padding[2]});
    } else {
        // Right section is big
        // 1. section
        renderers[0].display(
            style, {.position = {position[0] + layout.left_inset, 0.0f},
                    .size = {width[0] - layout.left_inset, layout.height},
                    .padding = padding[0]});
        // 2. section
        float t2_width =
            renderers[2].get_width_without_spacers(style, padding[2]);
        renderers[1].display(style,
                             {.position = {position[1], 0.0f},
                              .size = {width[1] - t2_width, layout.height},
                              .padding = padding[1]});
        renderers[2].display(style,
                             {.position = {layout.total_width - t2_width, 0.0f},
                              .size = {t2_width, layout.height},
                              .padding = padding[2]});
    }
}

static void toolbar_layout_three(ToolbarLayout layout,
                                 ToolbarStyle const& style,
                                 std::span<ToolbarRenderer> renderers,
                                 std::span<ToolbarPadding const> padding,
                                 std::span<int const> splits) {
    float position[3] = {layout.left_inset, (float)splits[0], (float)splits[1]};
    float width[3] = {(float)splits[0] - layout.left_inset,
                      (float)(splits[1] - splits[0]),
                      layout.total_width - splits[1]};
    for (std::size_t i = 0; i < 3; ++i)
        renderers[i].display(style, {.position = {position[i], 0.0f},
                                     .size = {width[i], layout.height},
                                     .padding = padding[i]});
}

static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

static std::array<ToolbarPadding, 3>
dockspace_resolve_toolbar_padding(vml::float2 in_padding, float item_spacing) {
    ToolbarPadding padding = resolve_toolbar_padding(in_padding);
    std::array<ToolbarPadding, 3> result = {padding, padding, padding};
    result[0].right = item_spacing / 2;
    result[1].left = item_spacing / 2;
    result[1].right = item_spacing / 2;
    result[2].left = item_spacing / 2;
    return result;
}

/// Displays the window in which the toolbar is drawn through \p content
static void toolbar_window(float2 position, float2 size,
                           std::span<int const> splits,
                           std::array<float, 2> insets,
                           std::span<ToolbarRenderer> renderers) {
    toolbar_bg_window(position, size);
    /// 6 is good looking with compact title bar style
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    bool open = begin_toolbar_window("Main_Toolbar", position, size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open) return;
    // Window content
    ToolbarLayout layout = {
        .height = size.y, .total_width = size.x, .left_inset = insets[0]};
    ToolbarStyle style = get_global_toolbar_style();
    style.flags |= ToolbarFlags::no_decoration;
    auto padding = dockspace_resolve_toolbar_padding(
        toolbar_style.padding, toolbar_style.item_spacing);
    switch (splits.size()) {
    case 0:
        toolbar_layout_one(layout, style, renderers, padding, splits);
        break;
    case 1:
        toolbar_layout_two(layout, style, renderers, padding, splits);
        break;
    case 2:
        toolbar_layout_three(layout, style, renderers, padding, splits);
        break;
    default:
        std::terminate();
    }
    if (ImGui::IsWindowFocused() && GImGui->WindowsFocusOrder.size() > 1 &&
        ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left]) {
        ImGui::FocusWindow(*(GImGui->WindowsFocusOrder.end() - 2));
    }
    ImGui::End();
}

static void display_toolbars(DockspaceImpl& impl, Window& window) {
    float height = window.titlebar_height();
    auto splits = find_toolbar_splits(window, impl.main_dock_id);
    float width = window.size().x;
    erase_if(splits, [&](int pos) {
        return pos >= width * 0.33 && pos <= width * 0.67;
    });
    assert(splits.size() <= 2);
    toolbar_window(ImGui::GetMainViewport()->Pos, {width, height}, splits,
                   {compute_left_inset(window), 0.0f}, impl.toolbar_renderers);
}
