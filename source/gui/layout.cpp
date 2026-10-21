#include "gui/layout.h"

#include <limits>
#include <utility>

#include <csp.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

using namespace mosaic;

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