#include "view_test.h"
#include "mosaic/gui/view.h"
#include "mosaic/gui/view_support.h"

#include <imgui.h>
#include <memory>
#include <mosaic/gui/containers.h>
#include <mosaic/gui/widgets.h>

using namespace mosaic;
using namespace ui;

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

    // Color
    vml::float3 color3 = {0.5f, 1.0f, 0.0f};
    vml::float4 color4 = {0.5f, 0.0f, 1.0f, 0.5f};

    // Conditional views
    bool one_arm_visible = false;
    bool two_arm_visible = false;
    int switch_index = 0;

    std::string long_text =
        R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur neque leo, ultricies in quam vel, bibendum tristique nisi. Pellentesque viverra ullamcorper dui sit amet lobortis. Nulla quis arcu lacus. Duis placerat id felis eu gravida. In et elit egestas, efficitur elit ac, dictum enim. Quisque hendrerit ac dui eu ullamcorper. Ut velit lectus, vulputate sit amet vestibulum quis, scelerisque at nisi. Integer vitae mi sapien. Duis tincidunt, purus vitae fringilla consectetur, lorem ligula mattis augue, vel accumsan tortor eros ut eros. Integer libero erat, bibendum ut efficitur quis, scelerisque vitae odio. Mauris tristique lacus a maximus congue. Integer posuere, urna efficitur tempus facilisis, elit ligula aliquet nulla, ut ornare lorem turpis at ante.
In congue felis in nisl mattis ornare. Suspendisse convallis sem quis sem consectetur elementum. Morbi lacinia ligula lectus, eget pulvinar odio maximus eu. Interdum et malesuada fames ac ante ipsum primis in faucibus. Fusce auctor velit ex, in volutpat nulla mattis et. Donec cursus nibh eu magna euismod accumsan. Nam sem nunc, posuere nec lacus nec, egestas gravida dui.
Aliquam scelerisque lacus ac nisl volutpat semper. Sed a nibh ac lorem placerat porta eu in tellus. Pellentesque aliquet diam at libero sodales, at bibendum leo tincidunt. Donec eu lobortis erat. Morbi facilisis orci erat, ultricies pharetra lorem consectetur eget. Phasellus tincidunt egestas condimentum. Mauris vel ligula eu odio mattis bibendum. Aenean tincidunt sem massa, a tincidunt magna mattis vitae. Phasellus sapien leo, gravida eu placerat pretium, volutpat vel tortor. Curabitur metus libero, ornare ac sagittis sed, imperdiet id justo. Duis et mattis orci. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae;
Duis mauris mi, dignissim in tincidunt vitae, egestas eget lorem. Praesent congue erat vel nulla dapibus fringilla. Cras vitae lobortis sem. Aenean semper nec nunc id ornare. Phasellus a sapien vitae justo egestas scelerisque eu non purus. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam hendrerit ultrices diam vel volutpat. Maecenas sit amet lacinia dui.
Etiam id eros at mauris sodales blandit non vulputate felis. Vivamus risus felis, scelerisque in nisi non, porta sodales odio. Nunc scelerisque laoreet eros ac rutrum. Donec interdum, elit eget fermentum sollicitudin, purus eros mollis neque, a pellentesque turpis massa non odio. Nullam varius dui tortor, eget varius augue malesuada vitae. Phasellus rutrum ligula purus, eu sodales augue molestie faucibus. Morbi mattis, ipsum vitae ultricies blandit, leo urna ultrices nibh, id sagittis felis orci ac velit. Vivamus mattis consequat nisl nec laoreet. Suspendisse a mollis dui. Fusce vitae sollicitudin odio, sed aliquam quam. Ut suscipit enim et mauris hendrerit, id lobortis nulla consequat. Praesent ac turpis eget ex tincidunt dapibus. Aliquam nec urna id leo tristique pellentesque non quis velit. Vestibulum finibus risus mattis efficitur pharetra. Quisque vel ullamcorper urna, id faucibus urna. )";
};

// =============================================================================
// Shared state-backed views
// =============================================================================

Owner<View> make_counter_text(TestState& state) {
    return text(computed([&state] {
        return std::string("Counter: ") + std::to_string(state.counter);
    }));
}

Owner<View> make_message_text(TestState const& state) {
    return text(computed([&state] { return state.message; }));
}

Owner<View> make_float_slider(TestState& state) {
    return slider<float>(bind(&state.float_value), 0.0f, 1.0f,
                         {.format = "%.2f"}) |
           hfill;
}

Owner<View> make_double_slider(TestState& state) {
    return slider<double>(bind(&state.double_value), 0.0, 1.0,
                          {.format = "%.3f"}) |
           hfill;
}

Owner<View> make_int_slider(TestState& state) {
    return slider<int>(bind(&state.int_value), 0, 100, {.format = "%d"}) |
           hfill;
}

Owner<View> make_unsigned_slider(TestState& state) {
    return slider<unsigned>(bind(&state.unsigned_value), 0u, 100u,
                            {.format = "%u"}) |
           hfill;
}

Owner<View> make_float2_slider(TestState& state) {
    return slider<vml::float2>(bind(&state.float2_value), 0.0f, 1.0f,
                               {.format = "%.2f"}) |
           hfill;
}

Owner<View> make_float3_slider(TestState& state) {
    return slider<vml::float3>(bind(&state.float3_value), 0.0f, 1.0f,
                               {.format = "%.2f"}) |
           hfill;
}

Owner<View> make_float4_slider(TestState& state) {
    return slider<vml::float4>(bind(&state.float4_value), 0.0f, 1.0f,
                               {.format = "%.3f"}) |
           hfill;
}

Owner<View> make_slider_examples(TestState& state) {
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

Owner<View> make_color_examples(TestState& state) {
    return vstack({
               text("Color editor"),
               color_edit(bind(&state.color3)),
               color_edit(bind(&state.color4)),
           }) |
           hfill;
}

// =============================================================================
// Sections
// =============================================================================

Owner<View> make_header(TestState const& state) {
    return hstack({
               text("Mosaic UI — View Test"),
               spacer(),
               make_message_text(state),
           }) |
           hfill;
}

Owner<View> make_basic_widgets(TestState& state) {
    auto actions = hstack({
                       button("Increment",
                              [&state] {
                                  ++state.counter;
                                  state.message = "Counter incremented";
                              }) |
                           enabled(bind(&state.buttons_enabled)),
                       button("Reset", [&state] { state = TestState{}; }) |
                           enabled(bind(&state.buttons_enabled)),
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

Owner<View> make_alignment_examples() {
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

Owner<View> make_weighted_columns() {
    auto make_column = [](std::string title, std::string label) -> Owner<View> {
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

Owner<View> make_scroll_examples() {
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

    Array<Column> cols;
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

Owner<View> make_tab_examples() {
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

static Owner<View> make_conditional_examples(TestState& state) {
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

static Owner<View> make_image_example() { return image(nullptr, 1.5); }

static Owner<View> make_wrapping_text_demo(TestState const& state) {
    return vscroll(
        {text("Wrapping text hfit"), hseparator(),
         text(bind(&state.long_text), {.wrap = true}) | hfit,

         hseparator(),

         text("Wrapping text hfill"), hseparator(),
         text(bind(&state.long_text), {.wrap = true}) | hfill,

         hseparator(),

         text("Wrapping text hfixed(300)"), hseparator(),
         text(bind(&state.long_text), {.wrap = true}) | fixed_width(300),

         hseparator(),

         text("Wrapping text in vstack"), hseparator(),
         vstack({
             text(bind(&state.long_text), {.wrap = true}),
         }) | hfit,

         hseparator(),

         text("Wrapping text in hstack"), hseparator(),
         hstack({
             text(bind(&state.long_text),
                  {.wrap = true, .default_wrap_width = 500}),
             vseparator(),
             text(bind(&state.long_text),
                  {.wrap = true, .default_wrap_width = 200}),
             vseparator(),
             text(bind(&state.long_text), {.wrap = true}),
         })});
}

// =============================================================================
// Stress Test
// =============================================================================

static Owner<View> make_layout_stress_tests() {
    // Case 1: fixed-height children whose combined requested height
    // exceeds the height available to the stack.
    auto fixed_overflow = vstack({
                              button("Fixed 120 px") | fixed_height(120.0f),
                              button("Fixed 100 px") | fixed_height(100.0f),
                              button("Fixed 80 px") | fixed_height(80.0f),
                          }) |
                          hfill | fixed_height(180.0f);

    // Case 2: fixed, fit, and fill children competing for limited space.
    auto mixed_overflow =
        vstack({
            button("Fixed 100 px") | fixed_height(100.0f),
            button("Fit to content") | vfit,
            button("Fill remaining space") | vfill,
            button("Another fixed child") | fixed_height(100.0f),
        }) |
        hfill | fixed_height(180.0f);

    // Case 3: overflow in an inner stack, itself constrained by
    // a parent stack.
    auto nested_overflow =
        vstack({
            vstack({
                button("Inner fixed 100 px") | fixed_height(100.0f),
                button("Inner fixed 100 px") | fixed_height(100.0f),
                button("Inner fixed 100 px") | fixed_height(100.0f),
            }) | hfill |
                fixed_height(140.0f),
            button("Following sibling"),
        }) |
        hfill | fixed_height(200.0f);

#if 0
    // Case 4: spacing alone consumes more than the available height.
    auto spacing_overflow = vstack({
        button("First"),
        button("Second"),
        button("Third"),
        button("Fourth"),
    }) | hfill | fixed_height(50.0f);
#endif

    return vscroll({
        text("Layout stress tests"),
        text("Inspect the child frames while these stacks are constrained."),
        hseparator(),
        text("Fixed children: overflow"),
        std::move(fixed_overflow),
        hseparator(),
        text("Mixed sizing: fixed / fit / fill"),
        std::move(mixed_overflow),
        hseparator(),
        text("Nested overflow"),
        std::move(nested_overflow),
#if 0
        hseparator(),
        text("Spacing overflow"),
        std::move(spacing_overflow),
#endif
    });
}

// =============================================================================
// Root
// =============================================================================

Owner<View> make_demo_content(TestState& state) {
    auto main_page = vscroll({make_header(state), hseparator(),

                              make_basic_widgets(state), hseparator(),

                              make_alignment_examples(), hseparator(),

                              make_slider_examples(state), hseparator(),

                              make_color_examples(state), hseparator(),

                              make_weighted_columns(), hseparator(),

                              make_scroll_examples(), hseparator(),

                              make_tab_examples(), hseparator(),

                              make_conditional_examples(state), hseparator(),

                              make_image_example()});
    auto wrapping_text = make_wrapping_text_demo(state);
    return tab_view({tab("Main", std::move(main_page)),
                     tab("Wrapping Text", std::move(wrapping_text)),
                     tab("Layout stress tests", make_layout_stress_tests())});
}

} // namespace

static std::pair<Owner<View>, std::shared_ptr<void>> render_test_view() {
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