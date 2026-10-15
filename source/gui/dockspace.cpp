#include "gui/dockspace.h"

#include <algorithm>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/function_view.hpp>
#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include "application/window.h"

using namespace mosaic;
using namespace vml::short_types;

static constexpr auto main_window_id = "MainWindow";
static constexpr auto main_dockspace_id = "MainWindow_Dockspace";

static void make_unique(auto& vector) {
    auto last = std::unique(vector.begin(), vector.end());
    vector.resize(last - vector.begin());
}

static void erase_if(auto& vector, auto pred) {
    auto itr = std::remove_if(vector.begin(), vector.end(), pred);
    vector.erase(itr, vector.end());
}

static void with_window_size_constraints(vml::float2 min_size, auto&& block) {
    ImGuiStyle& style = ImGui::GetStyle();
    utl::scope_guard restore = [&, old_size = style.WindowMinSize] {
        style.WindowMinSize = old_size;
    };
    style.WindowMinSize = min_size;
    block();
}

Dockspace::Dockspace() {
    for (std::size_t i = 0; i < 3; ++i)
        toolbar_renderers[i].set_toolbar(&toolbars[i]);
}

void Dockspace::display(Window* window) {
    if (!window) return;
    window->set_style({.full_size_content = true, .large_titlebar = true});
    main_window(window->titlebar_height());
    display_toolbars(*window);
}

ToolbarStyle const toolbar_style{.padding = {6.0f, 6.0f, 6.0f, 6.0f},
                                 .flags = ToolbarFlags::no_decoration};

void Dockspace::set_left_toolbar(Toolbar toolbar) {
    auto style = toolbar_style;
    style.padding.right = style.item_spacing / 2;
    toolbar.set_style(style);
    toolbars[0] = std::move(toolbar);
}

void Dockspace::set_center_toolbar(Toolbar toolbar) {
    auto style = toolbar_style;
    style.padding.left = style.item_spacing / 2;
    style.padding.right = style.item_spacing / 2;
    toolbar.set_style(style);
    toolbars[1] = std::move(toolbar);
}

void Dockspace::set_right_toolbar(Toolbar toolbar) {
    auto style = toolbar_style;
    style.padding.left = style.item_spacing / 2;
    toolbar.set_style(style);
    toolbars[2] = std::move(toolbar);
}

void Dockspace::take_focus() {
    if (!main_imgui_window) return;
    auto flags = ImGuiFocusRequestFlags_RestoreFocusedChild |
                 ImGuiFocusRequestFlags_UnlessBelowModal;
    ImGui::FocusWindow((ImGuiWindow*)main_imgui_window, flags);
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

void Dockspace::main_window(float toolbar_height) {
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
    with_window_size_constraints(min_window_size,
                                 [&] { main_dock_id = submit_dockspace(); });
    main_imgui_window = ImGui::GetCurrentWindowRead();
    ImGui::End();
    ImGui::PopStyleVar(6);
}

static float2 get_position() { return (float2)ImGui::GetMainViewport()->Pos; }

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

/// Displays the window in which the toolbar is drawn through \p content
static void toolbar_window(float2 position, float2 size,
                           utl::function_view<void()> content) {
    toolbar_bg_window(position, size);
    /// 6 is good looking with compact title bar style
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, 0);
    bool open = begin_toolbar_window("Main_Toolbar", position, size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open) return;
    content();
    if (ImGui::IsWindowFocused() && GImGui->WindowsFocusOrder.size() > 1 &&
        ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left]) {
        ImGui::FocusWindow(*(GImGui->WindowsFocusOrder.end() - 2));
    }
    ImGui::End();
}

static utl::small_vector<int, 2> get_toolbar_spacing(Window& window,
                                                     unsigned main_dock_id) {
    auto potentialRects = [&] {
        int2 window_pos = get_position();
        ImGuiDockNode const* main_node =
            ImGui::DockBuilderGetNode(main_dock_id);
        assert(main_node);
        utl::stack<ImGuiDockNode const*> stack;
        stack.push(main_node);
        std::vector<vml::rectangle<int>> result;
        while (!stack.empty()) {
            auto* node = stack.pop();
            if ((int)node->Pos.y == (int)main_node->Pos.y && node->IsVisible &&
                node != main_node &&
                (int2)node->Size != (int2)main_node->Size) {
                int2 min = (int2)node->Pos - window_pos;
                int2 max = min + (int2)node->Size;
                result.push_back({min, max});
            }
            if (!node->IsLeafNode()) {
                stack.push(node->ChildNodes[0]);
                stack.push(node->ChildNodes[1]);
            }
        }
        return result;
    }();
    return [&]() -> utl::small_vector<int, 2> {
        std::vector<int> positions;
        for (auto rect : potentialRects) {
            positions.push_back(rect.lower_bound().x);
            positions.push_back(rect.upper_bound().x);
        }
        std::sort(positions.begin(), positions.end());
        make_unique(positions);
        erase_if(positions,
                 [&](int p) { return p <= 0 || p >= window.size().x; });
        for (int i = 0; auto& p : positions) {
            if (i++ % 2 == 0) p += 1;
            else p -= 1;
        }
        make_unique(positions);
        switch (positions.size()) {
        case 0:
            return {};
        case 1:
            return {positions.front()};
        default:
            return {positions.front(), positions.back()};
        }
    }();
}

namespace {

struct ToolbarLayout {
    float height;
    float total_width;
    float left_inset;
};

} // namespace

static void toolbar_layout_one(ToolbarLayout layout,
                               std::span<ToolbarRenderer> renderers,
                               std::span<int const> /* spacing */) {
    float t0_width = renderers[0].get_width_without_spacers();
    float t2_width = renderers[2].get_width_without_spacers();
    ImGui::SetCursorPos({layout.left_inset, 0.0f});
    renderers[0].display({t0_width, layout.height});
    ImGui::SetCursorPos({layout.left_inset + t0_width, 0.0f});
    renderers[1].display(
        {layout.total_width - layout.left_inset - t0_width - t2_width,
         layout.height});
    ImGui::SetCursorPos({layout.total_width - t2_width, 0.0f});
    renderers[2].display({t2_width, layout.height});
}

static void toolbar_layout_two(ToolbarLayout layout,
                               std::span<ToolbarRenderer> renderers,
                               std::span<int const> spacing) {
    float spacer = spacing[0];
    float position[2] = {0, spacer};
    float width[2] = {spacer, layout.total_width - spacer};
    if (spacer >= layout.total_width / 2) {
        // Left section is big
        // 1. section
        float t0_width = renderers[0].get_width_without_spacers();
        ImGui::SetCursorPos({position[0] + layout.left_inset, 0.0f});
        renderers[0].display({t0_width, layout.height});
        ImGui::SetCursorPos({position[0] + layout.left_inset + t0_width, 0.0f});
        renderers[1].display(
            {width[0] - t0_width - layout.left_inset, layout.height});
        // 2. section
        ImGui::SetCursorPos({position[1], 0.0f});
        renderers[2].display({width[1], layout.height});
    } else {
        // Right section is big
        // 1. section
        ImGui::SetCursorPos({position[0] + layout.left_inset, 0.0f});
        renderers[0].display({width[0] - layout.left_inset, layout.height});
        // 2. section
        float t2_width = renderers[2].get_width_without_spacers();
        ImGui::SetCursorPos({position[1], 0.0f});
        renderers[1].display({width[1] - t2_width, layout.height});
        ImGui::SetCursorPos({layout.total_width - t2_width, 0.0f});
        renderers[2].display({t2_width, layout.height});
    }
}

static void toolbar_layout_three(ToolbarLayout layout,
                                 std::span<ToolbarRenderer> renderers,
                                 std::span<int const> spacing) {
    float position[3] = {layout.left_inset, (float)spacing[0],
                         (float)spacing[1]};
    float width[3] = {(float)spacing[0] - layout.left_inset,
                      (float)(spacing[1] - spacing[0]),
                      layout.total_width - spacing[1]};
    for (std::size_t i = 0; i < 3; ++i) {
        ImGui::SetCursorPos({position[i], 0.0f});
        renderers[i].display({width[i], layout.height});
    }
}

static float compute_left_inset(Window const& window) {
    auto title_buttons_area = window.title_buttons_area();
    return title_buttons_area.lower_bound().x +
           title_buttons_area.upper_bound().x;
}

void Dockspace::display_toolbars(Window& window) {
    float height = window.titlebar_height();
    auto spacing = get_toolbar_spacing(window, main_dock_id);
    float width = window.size().x;
    erase_if(spacing, [&](int pos) {
        return pos >= width * 0.33 && pos <= width * 0.67;
    });
    assert(spacing.size() <= 2);
    toolbar_window(get_position(), {width, height}, [&] {
        auto* toolbar_layout_fn =
            std::array{toolbar_layout_one, toolbar_layout_two,
                       toolbar_layout_three}[spacing.size()];
        toolbar_layout_fn({.height = height,
                           .total_width = (float)window.size().x,
                           .left_inset = compute_left_inset(window)},
                          toolbar_renderers, spacing);
    });
}
