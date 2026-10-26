#include "view_test.h"
#include "mosaic/gui/view_support.h"

#include <imgui.h>
#include <memory>
#include <mosaic/gui/containers.h>
#include <mosaic/gui/widgets.h>

using namespace mosaic::ui;

namespace {

struct TestState {
    int counter = 0;
    bool buttons_enabled = true;
    std::string message = "Ready";

    // Floating-point
    float float_value = 0.5f;
    double double_value = 0.25;

    // Integer
    int int_value = 25;
    unsigned unsigned_value = 50;

    // Vectors
    vml::float2 float2_value = {0.25f, 0.75f};
    vml::float3 float3_value = {0.2f, 0.5f, 0.8f};
    vml::float4 float4_value = {0.1f, 0.3f, 0.6f, 1.0f};

    // Conditional views
    bool one_arm_visible = false;
    bool two_arm_visible = false;
    int switch_index = 0;
};

// =============================================================================
// Shared state-backed views
// =============================================================================

Ref<View> make_counter_text(TestState& state) {
    return text(computed([state] {
        return std::string("Counter: ") + std::to_string(state.counter);
    }));
}

Ref<View> make_message_text(TestState& state) {
    return text(computed([state] { return state.message; }));
}

Ref<View> make_float_slider(TestState& state) {
    return slider<float>(bind(&state.float_value), 0.0f, 1.0f,
                         {.format = "%.2f"}) |
           hfill;
}

Ref<View> make_double_slider(TestState& state) {
    return slider<double>(bind(&state.double_value), 0.0, 1.0,
                          {.format = "%.3f"}) |
           hfill;
}

Ref<View> make_int_slider(TestState& state) {
    return slider<int>(bind(&state.int_value), 0, 100, {.format = "%d"}) |
           hfill;
}

Ref<View> make_unsigned_slider(TestState& state) {
    return slider<unsigned>(bind(&state.unsigned_value), 0u, 100u,
                            {.format = "%u"}) |
           hfill;
}

Ref<View> make_float2_slider(TestState& state) {
    return slider<vml::float2>(bind(&state.float2_value), 0.0f, 1.0f,
                               {.format = "%.2f"}) |
           hfill;
}

Ref<View> make_float3_slider(TestState& state) {
    return slider<vml::float3>(bind(&state.float3_value), 0.0f, 1.0f,
                               {.format = "%.2f"}) |
           hfill;
}

Ref<View> make_float4_slider(TestState& state) {
    return slider<vml::float4>(bind(&state.float4_value), 0.0f, 1.0f,
                               {.format = "%.3f"}) |
           hfill;
}

Ref<View> make_slider_examples(TestState& state) {
    return vstack({
               text("Floating-point sliders"),
               make_float_slider(state),
               make_double_slider(state),

               hseparator(),

               text("Integer sliders"),
               make_int_slider(state),
               make_unsigned_slider(state),

               hseparator(),

               text("Vector sliders"),
               make_float2_slider(state),
               make_float3_slider(state),
               make_float4_slider(state),
           }) |
           hfill;
}

// =============================================================================
// Sections
// =============================================================================

Ref<View> make_header(TestState& state) {
    return hstack({
               text("Mosaic UI — View Test"),
               spacer(),
               make_message_text(state),
           }) |
           hfill;
}

Ref<View> make_basic_widgets(TestState& state) {
    auto actions = hstack({
                       button("Increment",
                              [&state] {
                                  ++state.counter;
                                  state.message = "Counter incremented";
                              }),

                       button("Reset", [&state] { state = TestState{}; }),

                       make_counter_text(state),
                   }) |
                   hfill;

    auto checkbox_row = checkbox(bind(&state.buttons_enabled)) | align_left;

    return vstack({
        text("Basic widgets"),
        std::move(actions),
        std::move(checkbox_row),
    });
}

Ref<View> make_alignment_examples() {
    auto horizontal_alignment = hstack({
                                    button("Left") | align_left,
                                    spacer(),
                                    button("Center") | center,
                                    spacer(),
                                    button("Right") | align_right,
                                }) |
                                hfill | fixed_height(48.0f);

    auto sizing = hstack({
                      text("Fixed width"),
                      button("120 px") | fixed_width(120.0f),
                      button("Fill") | hfill,
                  }) |
                  hfill;

    return vstack({
        text("Sizing and alignment"),
        std::move(horizontal_alignment),
        std::move(sizing),
    });
}

Ref<View> make_weighted_columns() {
    auto make_column = [](std::string title, std::string label) -> Ref<View> {
        return vstack({
                   text(std::move(title)),
                   button(std::move(label)) | hfill,
               }) |
               hfill;
    };

    auto left = make_column("Weight 1", "A") | hfill_weight(1.0f);
    auto middle = make_column("Weight 2", "B") | hfill_weight(2.0f);
    auto right = make_column("Weight 1", "C") | hfill_weight(1.0f);

    return vstack({
               text("Flexible space"),

               hstack({
                   std::move(left),
                   vseparator(),
                   std::move(middle),
                   vseparator(),
                   std::move(right),
               }) | hfill |
                   vfill,
           }) |
           hfill;
}

Ref<View> make_scroll_examples() {

    ViewContainer<> horizontal_items;
    for (int i = 0; i < 20; ++i)
        horizontal_items.push_back(button("Item " + std::to_string(i)) | vfill);

    auto vertical_scroll = [j = 0] mutable {
        ViewContainer<> items;
        for (int i = 0; i < 20; ++i)
            items.push_back(button("Vertical item " + std::to_string(++j)));
        return vscroll(std::move(items)) | fixed_height(250.0f);
    };

    auto horizontal_scroll = hscroll(std::move(horizontal_items)) | vfit;

    std::vector<Column> cols;
    for (int i = 0; i < 4; ++i)
        cols.push_back(column(vertical_scroll()));

    return vstack({
               text("Scroll views"),

               text("Vertical scrolling"),
               vertical_scroll(),

               text("Horizontal scrolling"),
               std::move(horizontal_scroll),

               text("Scrolling in columns"),
               columns(std::move(cols)) | fixed_height(300.0f),
           }) |
           hfill;
}

Ref<View> make_tab_examples() {
    auto first_tab = vscroll({
        text("First tab"),
        text("This tab contains a vertical list of buttons."),
        hseparator(),

        [] {
            ViewContainer<> items;
            for (int i = 0; i < 12; ++i) {
                items.push_back(button("First tab item " + std::to_string(i)));
            }
            return vstack(std::move(items));
        }(),
    });

    auto second_tab = vstack({
                          text("Second tab"),
                          hstack({
                              button("Left"),
                              spacer(),
                              button("Right"),
                          }) | hfill,

                          checkbox(own(false)) | align_left,
                      }) |
                      hfill | vfill;

    auto third_tab = hstack({
                         text("A"),
                         vseparator(),
                         text("B"),
                         vseparator(),
                         text("C"),
                     }) |
                     hfill | vfill;

    return tab_view({
               tab("List", std::move(first_tab)),
               tab("Controls", std::move(second_tab)),
               tab("Columns", std::move(third_tab)),
           }) |
           hfill | fixed_height(260.0f);
}

static Ref<View> make_conditional_examples(TestState& state) {
    ViewContainer<> views;
    views.push_back(text("Conditional View with one arm"));
    views.push_back(hstack({
        text("Visible"),
        checkbox(bind(&state.one_arm_visible)),
    }));
    views.push_back(if_view(bind(&state.one_arm_visible),
                            hstack({button("Create"), button("Cancel")})));

    views.push_back(text("Conditional View with two arms"));
    views.push_back(hstack({
        text("Visible"),
        checkbox(bind(&state.two_arm_visible)),
    }));
    views.push_back(if_view(bind(&state.two_arm_visible),
                            hstack({button("Create"), button("Cancel")}),
                            text("Default")));

    int num_switch_cases = 10;
    views.push_back(text("Switch view"));
    views.push_back(hstack(
        {text("Index"),
         slider(computed([&] { return state.switch_index + 1; },
                         [&](int value) { state.switch_index = value - 1; }),
                1, num_switch_cases, {.format = "%d"})}));
    ViewContainer<> switch_cases;
    for (int i = 0; i < num_switch_cases; ++i)
        switch_cases.push_back(text("Switch Case " + std::to_string(i + 1)));
    views.push_back(
        switch_view(bind(&state.switch_index), std::move(switch_cases)));

    return vstack(std::move(views));
}

// =============================================================================
// Root
// =============================================================================

Ref<View> make_demo_content(TestState& state) {
    return vscroll({
        make_header(state),
        hseparator(),

        make_basic_widgets(state),
        hseparator(),

        make_alignment_examples(),
        hseparator(),

        make_slider_examples(state),
        hseparator(),

        make_weighted_columns(),
        hseparator(),

        make_scroll_examples(),
        hseparator(),

        make_tab_examples(),
        hseparator(),

        make_conditional_examples(state),
    });
}

} // namespace

static std::pair<Ref<View>, std::shared_ptr<void>> render_test_view() {
    auto state = std::make_shared<TestState>();
    auto root = make_demo_content(*state);

    return {std::move(root), std::move(state)};
}

void UiTest::display() {
    if (!m_open) return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    bool visible = ImGui::Begin("View Test", &m_open);
    ImGui::PopStyleVar();
    if (visible) {
        if (!m_root) std::tie(m_root, m_state) = render_test_view();
        vml::float2 pos = ImGui::GetCursorScreenPos();
        vml::float2 size = ImGui::GetContentRegionAvail();
        m_root->display({pos, size});
    }
    ImGui::End();
}