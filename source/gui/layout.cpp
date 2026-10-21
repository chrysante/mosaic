#include "gui/layout.h"

#include <csp.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

using namespace mosaic;

void ui::columns(char const* str_id, std::span<ColumnLayout const> columns,
                 ColumnLayoutOptions options) {
    if (columns.empty()) return;

    bool const have_headers = options.header_height != 0.0f;
    auto& style = ImGui::GetStyle();
    vml::float2 const window_padding = style.WindowPadding;
    float const splitter_width = style.SeparatorSize;
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

    auto distribute_delta = [&](float delta) {
        // Spread a width difference among columns that still have
        // capacity in the direction of the resize.
        for (int iteration = 0; iteration < (int)count + 1; ++iteration) {
            if (std::abs(delta) < 0.01f) break;
            int active = 0;
            for (std::size_t i = 0; i < count; ++i) {
                float const lo = ImMax(0.0f, columns[i].min_width);
                float const hi = ImMax(lo, columns[i].max_width);
                float const capacity =
                    delta > 0.0f ? hi - widths[i] : widths[i] - lo;
                if (capacity > 0.001f) ++active;
            }
            if (active == 0) break;
            float const share = delta / static_cast<float>(active);
            float applied = 0.0f;
            for (std::size_t i = 0; i < count; ++i) {
                float const lo = ImMax(0.0f, columns[i].min_width);
                float const hi = ImMax(lo, columns[i].max_width);
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
        distribute_delta(usable_width - sum);
        storage->SetInt(count_id, count);
    } else {
        for (std::size_t i = 0; i < count; ++i)
            widths[i] = clamp_width(i, storage->GetFloat(width_ids[i], 0.0f));
        float sum = 0.0f;
        for (float width : widths)
            sum += width;
        // Adapt to parent-window resizing.
        distribute_delta(usable_width - sum);
    }

    // ------------------------------------------------------------
    // Phase 1: Process splitters before drawing headers and bodies.
    // widths[] already contains the current widths.
    // ------------------------------------------------------------
    float x = origin.x;
    for (std::size_t i = 0; i < splitter_count; ++i) {
        x += widths[i];
        ImRect bb(ImVec2(x, origin.y),
                  ImVec2(x + splitter_width, origin.y + total_size.y));
        // SplitterBehavior expects the ID and rectangle of the
        // separator, plus the axis and adjacent sizes.
        ImGuiID const splitter_id =
            ImGui::GetID(("##splitter_" + std::to_string(i)).c_str());
        float width1 = widths[i];
        float width2 = widths[i + 1];
        ImGui::SplitterBehavior(bb, splitter_id, ImGuiAxis_X, &width1, &width2,
                                columns[i].min_width, columns[i + 1].min_width,
                                ImMax(2.0f, window_padding.x), 0.0f);
        // Respect both minimum and maximum widths.
        // Clamp the pair while preserving their combined width.
        float const pair_width = widths[i] + widths[i + 1];
        float const min1 = ImMax(0.0f, columns[i].min_width);
        float const max1 = ImMax(min1, columns[i].max_width);
        float const min2 = ImMax(0.0f, columns[i + 1].min_width);
        float const max2 = ImMax(min2, columns[i + 1].max_width);
        float const low = ImMax(min1, pair_width - max2);
        float const high = ImMin(max1, pair_width - min2);
        float new_width = ImClamp(width1, low, high);
        // Update the accumulation to account for the changed width
        x += new_width - widths[i];
        widths[i] = new_width;
        widths[i + 1] = pair_width - new_width;
        x += splitter_width;
    }
    // Persist final widths after handling all splitter interactions.
    for (std::size_t i = 0; i < count; ++i)
        storage->SetFloat(width_ids[i], widths[i]);
    storage->SetFloat(total_id, total_size.x);

    // ------------------------------------------------------------
    // Phase 2: render fixed headers using the UPDATED widths.
    // ------------------------------------------------------------
    if (have_headers) {
        x = 0.0f;
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
    x = 0.0f;
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