#include "gui/layout.h"

#include <cfloat>
#include <cstdio>
#include <limits>
#include <stack>
#include <string>
#include <utility>

#include <csp.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include "application/debug_settings.h"
#include "gui/font.h"

using namespace mosaic;

// =============================================================================
// Columns
// =============================================================================

namespace {

void distribute_delta(
    std::span<ui::ColumnLayout const> columns, std::span<float> widths,
    std::size_t begin, std::size_t end, float delta, float max_layout_width,
    std::size_t preferred = std::numeric_limits<std::size_t>::max()) {
    if (begin >= end || std::abs(delta) < 0.01f) return;
    auto get_bounds = [&](std::size_t i) {
        float const lo = ImMax(0.0f, columns[i].min_width);
        float const hi =
            ImMax(lo, ImMin(columns[i].max_width, max_layout_width));
        return std::pair{lo, hi};
    };
    // First, let the preferred column absorb as much of the delta
    // as possible. Other columns remain untouched if it can absorb
    // the entire change.
    if (preferred >= begin && preferred < end) {
        auto const [lo, hi] = get_bounds(preferred);
        float const old_width = widths[preferred];
        widths[preferred] = ImClamp(old_width + delta, lo, hi);
        delta -= widths[preferred] - old_width;
        if (std::abs(delta) < 0.01f) return;
    }
    // Redistribute only the remaining delta among the other columns.
    for (std::size_t iteration = 0; iteration < end - begin + 1; ++iteration) {
        if (std::abs(delta) < 0.01f) break;
        std::size_t active = 0;
        for (std::size_t i = begin; i < end; ++i) {
            if (i == preferred) continue;
            auto const [lo, hi] = get_bounds(i);
            float const capacity =
                delta > 0.0f ? hi - widths[i] : widths[i] - lo;
            if (capacity > 0.001f) ++active;
        }
        if (active == 0) break;
        float const share = delta / static_cast<float>(active);
        float applied = 0.0f;
        for (std::size_t i = begin; i < end; ++i) {
            if (i == preferred) continue;
            auto const [lo, hi] = get_bounds(i);
            float const capacity =
                delta > 0.0f ? hi - widths[i] : widths[i] - lo;
            if (capacity <= 0.001f) continue;
            float const amount = delta > 0.0f ? ImMin(share, capacity)
                                              : -ImMin(-share, capacity);
            widths[i] += amount;
            applied += amount;
        }
        if (std::abs(applied) < 0.001f) break;
        delta -= applied;
    }
}

} // namespace

void ui::columns(char const* str_id, std::span<ColumnLayout const> columns,
                 ColumnLayoutOptions options) {
    if (columns.empty()) return;

    bool const have_headers = options.header_height != 0.0f;
    auto& style = ImGui::GetStyle();
    vml::float2 const window_padding = style.WindowPadding;
    float const splitter_width = 2.0f * style.SeparatorSize;
    float const header_height = [&] {
        // No headers?
        if (options.header_height == 0.0f) return 0.0f;
        // Custom header heigt?
        if (options.header_height > 0.0f) return options.header_height;
        return ImGui::GetFontSize() + 2.0f * window_padding.y +
               style.SeparatorSize;
    }();

    std::size_t count = columns.size();
    std::size_t const splitter_count = count - 1;
    vml::float2 const origin = (vml::float2)ImGui::GetCursorScreenPos();
    vml::float2 const total_size = (vml::float2)ImGui::GetContentRegionAvail();
    const float usable_width =
        ImMax(0.0f, total_size.x - splitter_width * splitter_count);
    ImGuiID const count_id = ImGui::GetID("##column_count");
    ImGuiID const total_id = ImGui::GetID("##column_total_width");
    utl::small_vector<ImGuiID, 8> const width_ids = [count] {
        utl::small_vector<ImGuiID, 8> ids;
        ids.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            ImGui::PushID((int)i);
            ids.push_back(ImGui::GetID("##column_width"));
            ImGui::PopID();
        }
        return ids;
    }();
    utl::small_vector<float, 8> widths(count);

    auto clamp_width = [&](std::size_t i, float width) {
        float const lo = ImMax(0.0f, columns[i].min_width);
        float const hi = ImMax(lo, columns[i].max_width);
        return ImClamp(width, lo, hi);
    };

    ImGui::PushID(str_id);
    utl::scope_guard id_guard = [] { ImGui::PopID(); };

    ImGuiStorage* storage = ImGui::GetStateStorage();
    // Keep widths in ImGui's per-window state storage.
    // Widths are stored in pixels; resizing the parent redistributes
    // the difference while respecting the column constraints.
    bool const is_init = (std::size_t)storage->GetInt(count_id, -1) == count &&
                         storage->GetFloat(total_id, -1.0f) >= 0.0f;

    if (!is_init) {
        float const initial_width = usable_width / static_cast<float>(count);
        for (std::size_t i = 0; i < count; ++i)
            widths[i] = clamp_width(i, initial_width);
        float sum = 0.0f;
        for (float width : widths)
            sum += width;
        distribute_delta(columns, widths, 0, count, usable_width - sum,
                         usable_width);
        storage->SetInt(count_id, count);
    } else {
        for (std::size_t i = 0; i < count; ++i)
            widths[i] = clamp_width(i, storage->GetFloat(width_ids[i], 0.0f));
        float sum = 0.0f;
        for (float width : widths)
            sum += width;
        // Adapt to parent-window resizing.
        distribute_delta(columns, widths, 0, count, usable_width - sum,
                         usable_width);
    }

    // ------------------------------------------------------------
    // Phase 1: Process splitter interactions, then render splitters.
    // Hitboxes use the original positions for the entire interaction
    // pass. Visual separators use the final positions.
    // ------------------------------------------------------------
    // Snapshot the widths so interaction hitboxes don't move mid-pass.
    utl::small_vector<float, 8> original_widths(count);
    for (std::size_t i = 0; i < count; ++i)
        original_widths[i] = widths[i];
    utl::small_vector<float, 8> splitter_xs(splitter_count);
    float splitter_x = origin.x;
    for (std::size_t i = 0; i < splitter_count; ++i) {
        splitter_x += original_widths[i];
        splitter_xs[i] = splitter_x;
        splitter_x += splitter_width;
    }
    // Capture colors before making SplitterBehavior's rendering
    // transparent.
    ImU32 const separator_color = ImGui::GetColorU32(ImGuiCol_Separator);
    ImU32 const hovered_color = ImGui::GetColorU32(ImGuiCol_SeparatorHovered);
    ImU32 const active_color = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
    utl::small_vector<ImU32, 8> splitter_colors(splitter_count);
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive, ImVec4(0, 0, 0, 0));
    for (std::size_t i = 0; i < splitter_count; ++i) {
        // Use the original group widths for this interaction pass.
        float left_width = 0.0f;
        float right_width = 0.0f;
        float left_min = 0.0f;
        float left_max = 0.0f;
        float right_min = 0.0f;
        float right_max = 0.0f;
        for (std::size_t j = 0; j <= i; ++j) {
            left_width += original_widths[j];
            float const lo = ImMax(0.0f, columns[j].min_width);
            float const hi =
                ImMax(lo, ImMin(columns[j].max_width, usable_width));
            left_min += lo;
            left_max += hi;
        }
        for (std::size_t j = i + 1; j < count; ++j) {
            right_width += original_widths[j];
            float const lo = ImMax(0.0f, columns[j].min_width);
            float const hi =
                ImMax(lo, ImMin(columns[j].max_width, usable_width));
            right_min += lo;
            right_max += hi;
        }
        float const pair_width = left_width + right_width;
        float const effective_left_min =
            ImMax(left_min, pair_width - right_max);
        float const effective_right_min =
            ImMax(right_min, pair_width - left_max);
        ImRect const bb(
            ImVec2(splitter_xs[i], origin.y),
            ImVec2(splitter_xs[i] + splitter_width, origin.y + total_size.y));
        ImGuiID const splitter_id =
            ImGui::GetID(("##splitter_" + std::to_string(i)).c_str());
        float new_left_width = left_width;
        float new_right_width = right_width;
        ImGui::SplitterBehavior(bb, splitter_id, ImGuiAxis_X, &new_left_width,
                                &new_right_width, effective_left_min,
                                effective_right_min,
                                ImMax(2.0f, window_padding.x), 0.0f);
        // Capture interaction state immediately, while this splitter
        // is still the last submitted item.
        bool const hovered = ImGui::IsItemHovered();
        bool const active = ImGui::IsItemActive();
        splitter_colors[i] = active    ? active_color
                             : hovered ? hovered_color
                                       : separator_color;
        // Only the active splitter can change the layout.
        // Redistribute its delta across both groups, respecting limits.
        if (active) {
            float const delta = new_left_width - left_width;
            distribute_delta(columns, widths, 0, i + 1, delta, usable_width, i);
            distribute_delta(columns, widths, i + 1, count, -delta,
                             usable_width, i + 1);
        }
    }
    ImGui::PopStyleColor(3);

    // Draw all separators at their final positions.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float final_x = origin.x;
    for (std::size_t i = 0; i < splitter_count; ++i) {
        final_x += widths[i];
        vml::float2 min = {final_x, origin.y};
        vml::float2 max = {final_x + splitter_width, origin.y + total_size.y};
        draw_list->AddRectFilled(min, max, splitter_colors[i]);
        final_x += splitter_width;
    }

    // Persist final widths after handling all splitter interactions.
    for (std::size_t i = 0; i < count; ++i)
        storage->SetFloat(width_ids[i], widths[i]);
    storage->SetFloat(total_id, total_size.x);

    // ------------------------------------------------------------
    // Phase 2: render fixed headers using the UPDATED widths.
    // ------------------------------------------------------------
    if (have_headers) {
        float x = 0.0f;
        for (std::size_t i = 0; i < count; ++i) {
            ImGui::PushID((int)i);
            auto const window_flags = ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
            ImGui::SetCursorScreenPos({origin.x + x, origin.y});
            ImGui::BeginChild(
                "##column_header", ImVec2(widths[i], header_height),
                ImGuiChildFlags_AlwaysUseWindowPadding, window_flags);
            // clang-format off
            std::visit(csp::overload{
                [&](std::string const& text) {
                    ImGui::TextEx(text.data(), text.data() + text.size());
                },
                [&](std::function<void()> const& fn) {
                    if (fn) fn();
                }
            }, columns[i].header);
            // clang-format on
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + window_padding.y -
                                 style.ItemSpacing.y);
            ImGui::Separator();
            ImGui::EndChild();
            ImGui::PopID();
            x += widths[i];
            if (i < splitter_count) x += splitter_width;
        }
    }
    // ------------------------------------------------------------
    // Phase 3: render independently scrolling bodies.
    // ------------------------------------------------------------
    float const body_height = ImMax(0.0f, total_size.y - header_height);
    float x = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        ImGui::PushID((int)i);
        ImGui::SetCursorScreenPos({origin.x + x, origin.y + header_height});
        // ImGui::PushStyleVarY(ImGuiStyleVar_WindowPadding, 0.0f);
        ImGui::BeginChild("##column_body", ImVec2(widths[i], body_height),
                          ImGuiChildFlags_AlwaysUseWindowPadding);
        // ImGui::PopStyleVar();
        if (columns[i].content) columns[i].content();
        ImGui::EndChild();
        ImGui::PopID();
        x += widths[i];
        if (i < splitter_count) x += splitter_width;
    }
}

void ui::columns(char const* str_id,
                 std::initializer_list<ColumnLayout> columns,
                 ColumnLayoutOptions options) {
    ui::columns(str_id, std::span(columns.begin(), columns.end()), options);
}

// =============================================================================
// Properties
// =============================================================================

namespace {

//     Window
//     ┌───────────────────────────────────────────────────────────────────────┐
//     │        ┬                                                              │
//     │        │ outer_padding.y                                              │
//     │        ┴                                                              │
//     │        <section-title (optional)>                                     │
//     │       ┌───────────────────────────────────────────────────────┐       │
//     │  OPX  │       ┬                                               │  OPX  │
//     │ ├───┤ │       │  section_padding.y                            │ ├───┤ │
//     │       │  SPX  ┴                                          SPX  │       │
//     │       │ ├───┤ <label>                            <item> ├───┤ │       │
//     │       │       │ row_spacing ───────────────────────────       │       │
//     │       │       <label>                            <item>       │       │
//     │       │       ┬                                               │       │
//     │       │       │ section_padding.y                             │       │
//     │       │       ┴                                               │       │
//     │       └───────────────────────────────────────────────────────┘       │
//     │        ┬                                                              │
//     │        │ section_spacing                                              │
//     │        ┴                                                              │
//     │        <section-title (optional)>                                     │
//     │       ┌───────────────────────────────────────────────────────┐       │
//     │       │                                                       │       │
//     │       └───────────────────────────────────────────────────────┘       │
//     │        ┬                                                              │
//     │        │ outer_padding.y                                              │
//     │        ┴                                                              │
//     └───────────────────────────────────────────────────────────────────────┘
//
// OPX = outer_padding.x
// SPX = section_padding.x
//

struct PropertiesStyle {
    vml::float2 outer_padding = {5.0f, 5.0f};

    float section_spacing = 10.0f;
    vml::float2 section_padding = {10.0f, 10.0f};
    float section_rounding = 15.0f;
    float section_midpoint = 0.3;

    float row_spacing = 12.0f;
    bool draw_separators = true;
    float separator_alpha_mul = 0.4f;

    bool draw_section_backgrounds = true;
    ImU32 section_background = IM_COL32(255, 255, 255, 15);
};

} // namespace

static PropertiesStyle properties_style;

namespace {

struct Row {
    float top = 0.0f;
    float bottom = 0.0f;
};

struct SectionData {
    float top = 0.0f;
    float bottom = 0.0f;
    bool has_content = false;
};

struct Context {
    Context(ImDrawList* draw_list, vml::float2 origin, float content_width)
        : draw_list(draw_list)
        , start_y(origin.y)
        , cursor_y(start_y)
        , content_left(origin.x)
        , content_right(content_left + content_width)
        , content_width(content_width) {}

    ImDrawList* draw_list = nullptr;
    ImDrawListSplitter splitter;

    // Coordinates are screen coordinates!
    float start_y = 0.0f;
    float cursor_y = 0.0f;
    float content_left = 0.0f;
    float content_right = 0.0f;
    float content_width = 0.0f;

    float label_width() const {
        return content_width * properties_style.section_midpoint;
    }
    float item_width() const {
        return content_width * (1.0f - properties_style.section_midpoint);
    }

    SectionData current_section;
    int num_sections = 0;
};

} // namespace

static std::stack<Context> g_context_stack;

static Context& current_context() {
    assert(!g_context_stack.empty());
    return g_context_stack.top();
}

static Context& push_context(ImDrawList* draw_list, vml::float2 origin,
                             float content_width) {
    g_context_stack.emplace(draw_list, origin, content_width);
    return current_context();
}

static void pop_context() { g_context_stack.pop(); }

static void with_intersected_clip_rect(ImDrawList* DL, vml::float2 min,
                                       vml::float2 max, auto&& callback) {
    min = vml::max(min, (vml::float2)DL->GetClipRectMin());
    max = vml::min(max, (vml::float2)DL->GetClipRectMax());
    if (min.x >= max.x || min.y >= max.y) return;
    DL->PushClipRect(min, max);
    callback();
    DL->PopClipRect();
}

static void draw_section_background(Context& ctx) {
    float const x0 = ctx.content_left - properties_style.section_padding.x;
    float const x1 = ctx.content_right + properties_style.section_padding.x;
    ctx.splitter.SetCurrentChannel(ctx.draw_list, 0);
    ctx.draw_list->AddRectFilled(ImVec2(x0, ctx.current_section.top),
                                 ImVec2(x1, ctx.current_section.bottom),
                                 properties_style.section_background,
                                 properties_style.section_rounding);
    ctx.splitter.SetCurrentChannel(ctx.draw_list, 1);
}

static void finish_section(Context& ctx) {
    assert(ctx.current_section.has_content);
    ctx.cursor_y += properties_style.section_padding.y;
    ctx.current_section.bottom = ctx.cursor_y;
    if (properties_style.draw_section_backgrounds) draw_section_background(ctx);
    ctx.num_sections += 1;
}

static void start_section(Context& ctx, const char* title) {
    if (ctx.current_section.has_content) finish_section(ctx);
    if (ctx.num_sections > 0) ctx.cursor_y += properties_style.section_spacing;
    // Draw title
    if (title) {
        auto font_desc = FontDesc::ui_default().set_weight(FontWeight::bold);
        ImGui::SetCursorScreenPos({ctx.content_left, ctx.cursor_y});
        ui::with_font(font_desc, [&] { ImGui::TextEx(title); });
        float font_size = ImGui::GetFontSize();
        ctx.cursor_y += font_size + 0.5f * properties_style.row_spacing;
    }
    ctx.current_section = {.top = ctx.cursor_y, .has_content = false};
}

void ui::begin_properties(const char* str_id) {
    auto* window = ImGui::GetCurrentWindow();
    auto* draw_list = window->DrawList;
    float window_width = window->Size.x - 2.0f * window->WindowPadding.x;
    vml::float2 origin = ImGui::GetCursorScreenPos();
    float xpadding =
        properties_style.section_padding.x + properties_style.outer_padding.x;
    origin.x += xpadding;
    origin.y += properties_style.outer_padding.y;
    float content_width = window_width - 2.0f * xpadding;
    auto& ctx = push_context(draw_list, origin, content_width);
    // Channel 0: deferred backgrounds.
    // Channel 1: normal widget commands.
    ctx.splitter.Split(ctx.draw_list, 2);
    ctx.splitter.SetCurrentChannel(ctx.draw_list, 1);
    ImGui::PushID(str_id);
    start_section(ctx, nullptr);
}

void ui::end_properties() {
    auto& ctx = current_context();
    if (ctx.current_section.has_content) {
        finish_section(ctx);
        ctx.cursor_y += properties_style.outer_padding.y;
        ImGui::SetCursorScreenPos(ImVec2(ctx.content_left, ctx.cursor_y));
        ImGui::Dummy({0.0f, 0.0f});
    }
    ctx.splitter.Merge(ctx.draw_list);
    ImGui::PopID();
    pop_context();
}

void ui::section(const char* title) { start_section(current_context(), title); }

static void align_right(float right_edge, float preferred_width,
                        std::span<utl::function_view<void()> const> items);

void ui::internal::item_impl(
    const char* label, std::span<utl::function_view<void()> const> lines) {
    assert(!lines.empty());
    Context& ctx = current_context();
    auto* DL = ctx.draw_list;
    auto& imgui_style = ImGui::GetStyle();
    ctx.cursor_y += ctx.current_section.has_content
                        ? properties_style.row_spacing
                        : properties_style.section_padding.y;
    // Draw separator before next item
    if (properties_style.draw_separators && ctx.current_section.has_content) {
        DL->AddLineH(ctx.content_left, ctx.content_right,
                     ctx.cursor_y - 0.5f * properties_style.row_spacing,
                     ImGui::GetColorU32(ImGuiCol_Separator,
                                        properties_style.separator_alpha_mul),
                     ImGui::GetStyle().SeparatorSize);
    }
    // Submit the label into the content channel.
    if (label && label[0] != '\0') {
        auto* DL = ctx.draw_list;
        vml::float2 label_pos = {ctx.content_left + imgui_style.FramePadding.x,
                                 ctx.cursor_y + imgui_style.FramePadding.y};
        vml::float2 label_end =
            label_pos +
            vml::float2(ctx.label_width() - 2.0f * imgui_style.FramePadding.x,
                        ImGui::GetFontSize());
        with_intersected_clip_rect(DL, label_pos, label_end, [&] {
            DL->AddText(nullptr, 0.0f, label_pos,
                        ImGui::GetColorU32(ImGuiCol_Text), label, nullptr);
        });
    }
    // We set cursor position here because at least one item will be submitted
    // by align_right()
    vml::float2 cursor_pos = {ctx.content_left, ctx.cursor_y};
    ImGui::SetCursorScreenPos(cursor_pos);
    vml::float2 item_clip_min = {ctx.content_left + ctx.label_width(),
                                 -FLT_MAX};
    vml::float2 item_clip_max = {
        ctx.content_right + properties_style.section_padding.x, FLT_MAX};
    with_intersected_clip_rect(DL, item_clip_min, item_clip_max, [&] {
        align_right(ctx.content_right, ctx.item_width(), lines);
    });
    ctx.cursor_y = ImGui::GetCursorScreenPos().y - imgui_style.ItemSpacing.y;
    ctx.current_section.has_content = true;
}

namespace {
struct ItemMeasureOptions {
    bool show_window = false;
    bool enable_inputs = false;
};
} // namespace

static ItemMeasureOptions measure_options;

static void begin_hidden_window(char const* id, ImVec2 size) {
    int window_flags = ImGuiWindowFlags_NoFocusOnAppearing;
    if (!measure_options.enable_inputs) {
        window_flags |= ImGuiWindowFlags_NoNavInputs |
                        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar;
    }
    ImGui::SetNextWindowSize({size.x, 100});
    ImGui::Begin(id, nullptr, window_flags);
    ImGui::GetCurrentWindow()->Hidden = !measure_options.show_window;
}

static vml::float2 measure_item_size(utl::function_view<void()> item,
                                     float preferred_width) {
    begin_hidden_window("hidden-window", ImGui::GetWindowSize());
    ImGui::PushItemWidth(preferred_width);
    item();
    ImGui::PopItemWidth();
    ImVec2 const size = ImGui::GetItemRectSize();
    ImGui::End();
    return size;
}

static void do_align_right(float right_edge, float preferred_width,
                           utl::function_view<void()> item) {
    vml::float2 item_size = measure_item_size(item, preferred_width);
#ifndef NDEBUG
    if (want_debug_overlays()) {
        auto cp = ImGui::GetCursorScreenPos();
        auto* DL = ImGui::GetWindowDrawList();
        char buf[256];
        std::snprintf(buf, sizeof buf, "%.0f, %.0f", item_size.x, item_size.y);
        DL->AddText(cp, IM_COL32(255, 0, 255, 255), buf);
    }
#endif
    vml::float2 cursor_pos = {right_edge - item_size.x,
                              ImGui::GetCursorScreenPos().y};
    ImGui::SetCursorScreenPos(cursor_pos);
    ImGui::PushItemWidth(preferred_width);
    item();
    ImGui::PopItemWidth();
}

static void align_right(float right_edge, float preferred_width,
                        std::span<utl::function_view<void()> const> items) {
    for (auto item : items)
        do_align_right(right_edge, preferred_width, item);
}

#ifndef NDEBUG
static void draw_item_measurement_debugger() {
    ImGui::Checkbox("Show Window", &measure_options.show_window);
    ImGui::Checkbox("Enabled Inputs", &measure_options.enable_inputs);
}

static void draw_style_editor(PropertiesStyle& style) {
    ImGui::SeparatorText("General");
    ImGui::SliderFloat2("Outer Padding", style.outer_padding.data(), 0.0f,
                        50.0f, "%.0f px");

    ImGui::SeparatorText("Sections");
    ImGui::SliderFloat("Section spacing", &style.section_spacing, 0.0f, 30.0f,
                       "%.0f px");
    ImGui::SliderFloat2("Section padding", style.section_padding.data(), 0.0f,
                        32.0f, "%.0f px");
    ImGui::SliderFloat("Section rounding", &style.section_rounding, 0.0f, 30.0f,
                       "%.0f px");
    ImGui::Checkbox("Draw section backgrounds",
                    &style.draw_section_backgrounds);
    ImVec4 color = ImGui::ColorConvertU32ToFloat4(style.section_background);
    if (ImGui::ColorEdit4("Section background", &color.x,
                          ImGuiColorEditFlags_AlphaBar |
                              ImGuiColorEditFlags_AlphaPreviewHalf))
        style.section_background = ImGui::ColorConvertFloat4ToU32(color);
    ImGui::SliderFloat("Section Midpoint", &style.section_midpoint, 0, 1,
                       "%.2f");

    ImGui::SeparatorText("Rows");
    ImGui::SliderFloat("Row spacing", &style.row_spacing, 0.0f, 20.0f,
                       "%.0f px");
    ImGui::Checkbox("Draw separators", &style.draw_separators);
    ImGui::SliderFloat("Separator Alpha Mul", &style.separator_alpha_mul, 0.0f,
                       1.0f, "%.1f");

    ImGui::Separator();
    if (ImGui::Button("Reset to defaults")) style = PropertiesStyle{};
}

void ui::draw_properties_debugger() {
    if (ImGui::TreeNodeEx("Item Measurement")) {
        draw_item_measurement_debugger();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Style", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_style_editor(properties_style);
        ImGui::TreePop();
    }
}
#endif