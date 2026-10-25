#include "mosaic/gui/view_support.h"
#include <cfloat>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include <mosaic/application/application.h>
#include <mosaic/application/application_settings.h>
#include <mosaic/application/input.h>
#include <mosaic/application/window.h>
#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/containers.h>
#include <mosaic/gui/dockspace.h>
#include <mosaic/gui/font.h>
#include <mosaic/gui/layout.h>
#include <mosaic/gui/panel.h>
#include <mosaic/gui/panel_manager.h>
#include <mosaic/gui/panel_registry.h>
#include <mosaic/gui/run_mode_controller.h>
#include <mosaic/gui/style_editor.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/gui/ui_context.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/widgets.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/service_context.h>

using namespace mosaic;

static void menubar();
static void display_properties_test(bool* open);
static void draw_test_properties(char const* str_id);
static void display_icon_viewer(bool* open);
static void display_view_test(bool* open);

static bool SliderFloatOnRelease(const char* label, float* value, float min,
                                 float max, const char* format = "%.3f",
                                 ImGuiSliderFlags flags = 0) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID(label);
    float* temp = storage->GetFloatRef(id, *value);
    // Keep the temporary value synchronized when not editing.
    if (!ImGui::IsAnyItemActive()) *temp = *value;
    ImGui::SliderFloat(label, temp, min, max, format, flags);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        *value = *temp;
        return true;
    }
    return false;
}

class ExamplePanel : public Panel {
public:
    ExamplePanel() {
        set_toolbar(make_toolbar());
        set_padding({0, 0});
    }

private:
    Toolbar make_toolbar() {
        // clang-format off
        return {
            ToolbarIconButton("link")
                .on_click([] {}), 
            CustomToolbarElement([this](vml::float2 size) {
                auto& style = ImGui::GetStyle();
                float item_height = ImGui::GetFontSize() + 
                                    2.0f * style.FramePadding.y;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 
                                     0.5f * (size.y - item_height));
                ImGui::SetNextItemWidth(size.x);
                ImGui::SliderInt("##NumColumns", &num_columns, 1, 10);
            }).width(100)
        };
        // clang-format on
    }

    void display() override {
        utl::small_vector<ui::ColumnLayout> columns;
        for (int i = 0; i < num_columns; ++i) {
            auto options = col_options[i];
            columns.push_back(
                {.header = i > 0 ? ui::ColumnLayout::Header(
                                       utl::strcat("Column ", i + 1))
                                 : ui::ColumnLayout::Header([] {
                                       ImGui::Text("Column 1");
                                       ImGui::SameLine();
                                       ImGui::SmallButton("Button");
                                   }),
                 .content = [this, i] { column(i); },
                 .min_width = options.min_width,
                 .max_width = options.max_width});
        }
        ui::columns("example-view-columns", columns,
                    {.header_height = use_header ? -1.0f : 0.0f});
    }

    void left_col() {
        ui::begin_properties("properties");
        bool have_toolbar = uses_toolbar();
        ui::item("Toolbar", [&] {
            if (ImGui::Checkbox("##toolbar", &have_toolbar)) {
                if (have_toolbar) set_toolbar(make_toolbar());
                else remove_toolbar();
            }
        });
        ui::item("Use Header",
                 [&] { ImGui::Checkbox("##use-header", &use_header); });

        float v = (std::sin((float)ImGui::GetTime()) + 1.0f) / 2.0f;
        ui::item("", [&] {
            ImGui::ProgressBar(v, ImVec2(ImGui::CalcItemWidth(), 0));
        });

        int current_mode = run_mode_lease ? 1 : 0;
        int mode = current_mode;
        ui::item("Run Mode", [&] {
            ImGui::BeginGroup();
            ImGui::RadioButton("Event Driven", &mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Continuous", &mode, 1);
            ImGui::EndGroup();
        });
        if (mode != current_mode) {
            if (mode == 0) {
                run_mode_lease.reset();
            } else {
                if (auto* ui_context = get_current_ui_context())
                    run_mode_lease = ui_context->run_mode_controller
                                         .enter_continuous_run_mode();
            }
        }
        ui::end_properties();
    }

    void display_col_options(int index) {
        ui::begin_properties("col-options");

        auto& options = col_options[index];
        bool use_max = options.max_width != FLT_MAX;
        ui::item("Min Width", [&] {
            ImGui::BeginGroup();
            float width = ImGui::CalcItemWidth();
            width -= ImGui::GetFontSize() +
                     2 * ImGui::GetStyle().FramePadding.x +
                     ImGui::GetStyle().ItemSpacing.x;
            width = std::max(0.0f, width);
            ImGui::PushItemWidth(width);
            SliderFloatOnRelease("##min-width", &options.min_width, 0.0f,
                                 use_max ? options.max_width : 1000.f, "%.f",
                                 ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
            bool dummy = false;
            ImGui::Checkbox("##empty_checkbox", &dummy);
            ImGui::PopStyleVar();
            ImGui::EndGroup();
        });
        ui::item("Max Width", [&] {
            ImGui::BeginGroup();
            if (use_max) {
                float width = ImGui::CalcItemWidth();
                width -= ImGui::GetFontSize() +
                         2 * ImGui::GetStyle().FramePadding.x +
                         ImGui::GetStyle().ItemSpacing.x;
                width = std::max(0.0f, width);
                ImGui::PushItemWidth(width);
                SliderFloatOnRelease("##max-width", &options.max_width,
                                     options.min_width, 1000.f, "%.f",
                                     ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopItemWidth();
                ImGui::SameLine();
            }
            if (ImGui::Checkbox("##use-max", &use_max)) {
                if (use_max) options.max_width = 1000.f;
                else options.max_width = FLT_MAX;
            }
            ImGui::EndGroup();
        });

        ui::end_properties();
    }

    void column(int index) {
        display_col_options(index);
        if (index == 0) {
            left_col();
            return;
        }
        draw_test_properties("test-properties");
    }

    RunModeController::Lease run_mode_lease;
    bool use_header = true;
    int num_columns = 3;
    struct ColOptions {
        float min_width = 10;
        float max_width = FLT_MAX;
    };
    utl::hashmap<int, ColOptions> col_options;
};

MOSAIC_REGISTER_VIEW(ExamplePanel, {.name = "ExamplePanel"})

class ExampleApp : public Application {
public:
    ExampleApp()
        : event_receiver(service_context().make_event_receiver())
        , event_emitter(service_context().make_event_emitter()) {
        // clang-format off
        command_palette().add({
            .name = "My Command",
            .key_comb = {.key = Key::K, .mod_flags = ModFlags::control},
            .command = [this] { main_window()->zoom(); }
        });
        command_palette().add({
            .name = "Toggle Menubar",
            .key_comb = {.key = Key::F10}, 
            .command = [this] { dockspace.toggle_menubar(); }
        });
        dockspace.set_left_toolbar({
            ToolbarIconButton("plus")
                .on_click([this] { text = "Clicked toolbar button"; }),
            ToolbarSpacer(), 
            ToolbarIconButton("play")
                .on_click([this] { text = "Clicked toolbar button"; }),
        });
        dockspace.set_center_toolbar({
            ToolbarIconButton("attention")
                .on_click([this] { show_demo_window = true; })
                .enabled([this] { return !show_demo_window; }),
            ToolbarSpacer(),
            ToolbarButton("Other Button"),
            ToolbarIconButton("search")
        });
        dockspace.set_right_toolbar({
            ToolbarIconButton("bell")
                .on_click([this] { 
                    event_emitter.publish(PanelManager::OpenCommand{
                        .name = "ExamplePanel"
                    });
                }),
            ToolbarSpacer(),
            ToolbarIconButton("plus")
                .on_click([this] { 
                    event_emitter.publish(PanelManager::OpenCommand{
                        .name = "MosaicDemoPanel"
                    });
                })
        });
        // clang-format on
    }

private:
    void frame() override {
        dockspace.display(main_window(), menubar);
        if (ImGui::Begin("Example App")) {
            if (ImGui::Button("ImGui Demo Window")) show_demo_window = true;
            if (ImGui::Button("Application Settings")) show_app_settings = true;
            if (ImGui::Button("Properties Test")) show_properties_test = true;
            if (ImGui::Button("Style Editor")) show_style_editor = true;
            if (ImGui::Button("Icon Viewer")) show_icon_viewer = true;
            if (ImGui::Button("View Test")) show_view_test = true;
        }
        ImGui::End();

        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);
        if (show_properties_test)
            display_properties_test(&show_properties_test);
        if (show_app_settings) {
            if (ImGui::Begin("Application Settings", &show_app_settings))
                draw_application_settings(*this);
            ImGui::End();
        }
        if (show_style_editor) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            bool visible = ImGui::Begin("Style Editor", &show_style_editor);
            ImGui::PopStyleVar();
            if (visible) style_editor.draw();
            ImGui::End();
        }
        if (show_icon_viewer) display_icon_viewer(&show_icon_viewer);
        if (show_view_test) display_view_test(&show_view_test);
    }

    Dockspace dockspace;
    EventReceiver event_receiver;
    EventEmitter event_emitter;
    std::string text;
    bool show_demo_window = false;
    bool show_app_settings = false;
    bool show_properties_test = false;
    bool show_style_editor = false;
    bool show_icon_viewer = false;
    bool show_view_test = false;
    StyleEditor style_editor;
};

int main() {
    ExampleApp app;
    return app.run();
}

static void menubar() {
    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("New Project", "Ctrl+N");
        ImGui::MenuItem("Open Project...", "Ctrl+O");
        ImGui::MenuItem("Open Recent");
        ImGui::Separator();
        ImGui::MenuItem("Save", "Ctrl+S");
        ImGui::MenuItem("Save As...", "Ctrl+Shift+S");
        ImGui::MenuItem("Save All", "Ctrl+Alt+S");
        ImGui::Separator();

        if (ImGui::BeginMenu("Export")) {
            ImGui::MenuItem("Export as PNG");
            ImGui::MenuItem("Export as JPG");
            ImGui::MenuItem("Export as PDF");
            ImGui::MenuItem("Export as JSON");
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Settings", "Ctrl+,");
        ImGui::MenuItem("Exit", "Alt+F4");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Undo", "Ctrl+Z");
        ImGui::MenuItem("Redo", "Ctrl+Y");
        ImGui::Separator();
        ImGui::MenuItem("Cut", "Ctrl+X");
        ImGui::MenuItem("Copy", "Ctrl+C");
        ImGui::MenuItem("Paste", "Ctrl+V");
        ImGui::MenuItem("Duplicate", "Ctrl+D");
        ImGui::MenuItem("Select All", "Ctrl+A");
        ImGui::Separator();
        ImGui::MenuItem("Find", "Ctrl+F");
        ImGui::MenuItem("Replace", "Ctrl+H");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Toolbar");
        ImGui::MenuItem("Status Bar");
        ImGui::MenuItem("Grid");
        ImGui::MenuItem("Guides");
        ImGui::Separator();

        if (ImGui::BeginMenu("Theme")) {
            ImGui::MenuItem("Dark");
            ImGui::MenuItem("Light");
            ImGui::MenuItem("Classic");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Zoom")) {
            ImGui::MenuItem("50%");
            ImGui::MenuItem("75%");
            ImGui::MenuItem("100%");
            ImGui::MenuItem("150%");
            ImGui::MenuItem("200%");
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        ImGui::MenuItem("Command Palette", "Ctrl+Shift+P");
        ImGui::MenuItem("Search Everything", "Ctrl+Shift+F");
        ImGui::Separator();

        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Show Metrics");
            ImGui::MenuItem("Show Debug Log");
            ImGui::MenuItem("Show ID Stack Tool");
            ImGui::MenuItem("Inspect Style");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            ImGui::MenuItem("Build", "Ctrl+B");
            ImGui::MenuItem("Build and Run", "Ctrl+F5");
            ImGui::MenuItem("Clean Build");
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Preferences...");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("New Window");
        ImGui::MenuItem("Reset Layout");
        ImGui::Separator();
        ImGui::MenuItem("Hierarchy");
        ImGui::MenuItem("Inspector");
        ImGui::MenuItem("Console");
        ImGui::MenuItem("Asset Browser");
        ImGui::Separator();

        if (ImGui::BeginMenu("Layout")) {
            ImGui::MenuItem("Default");
            ImGui::MenuItem("Coding");
            ImGui::MenuItem("Debugging");
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Documentation");
        ImGui::MenuItem("Keyboard Shortcuts");
        ImGui::MenuItem("Report a Bug");
        ImGui::Separator();
        ImGui::MenuItem("About");
        ImGui::MenuItem("About Dear ImGui");
        ImGui::EndMenu();
    }
}

static void help_marker(char const* text) {
    IconData icon = ui::icon_data("info-circled");
    ImGui::PushFont(icon.font, 0.0f);
    ImGui::TextDisabled("%s", icon.utf8_rep.data());
    ImGui::PopFont();
    ImGui::SetItemTooltip("%s", text);
}

static void draw_test_properties(char const* str_id) {
    ui::begin_properties(str_id);

    static int section_count = 5;
    int count = 0;
    if (count++ < section_count) {
        static bool is_titled = true;
        ui::section(is_titled ? "General" : nullptr);
        static float value = 1.0f;
        ui::item(
            "Value", [&] { ImGui::SliderFloat("##value-1", &value, 0, 1); },
            [&] { ImGui::SliderFloat("##value-2", &value, 0, 2); });
        ui::item("Titled", [&] { ImGui::Checkbox("##cond", &is_titled); });
        ui::item("Section Count", [&] {
            ImGui::InputInt("##count", &section_count);
            if (section_count < 1) section_count = 1;
        });
        static char name[128] = "My Object";
        ui::item("Name",
                 [&] { ImGui::InputText("##name", name, sizeof(name)); });
        static float opacity = 0.75f;
        ui::item("Opacity", [&] {
            ImGui::SliderFloat("##opacity", &opacity, 0, 1, "%.2f");
        });
    }
    if (count++ < section_count) {
        ui::section("Appearance");
        static ImVec4 color = ImVec4(0.25f, 0.6f, 0.9f, 1.0f);
        ui::item("Color", [&] { ImGui::ColorEdit4("##color", &color.x); });
        static int mode = 0;
        ui::item("Blend Mode", [&] {
            const char* modes[] = {"Opaque", "Alpha", "Additive", "Multiply"};
            ImGui::Combo("##blend-mode", &mode, modes, IM_ARRAYSIZE(modes));
        });
        static float size[2] = {128.0f, 64.0f};
        ui::item("Size", [&] {
            ImGui::DragFloat2("##size", size, 1.0f, 0.0f, 2048.0f);
        });
        static float rotation = 0.0f;
        ui::item("Rotation",
                 [&] { ImGui::SliderAngle("##rotation", &rotation); });
        static bool wireframe = false;
        static bool shadows = true;
        ui::item(
            "Rendering", [&] { ImGui::Checkbox("Wireframe", &wireframe); },
            [&] { ImGui::Checkbox("Shadows", &shadows); });
    }
    if (count++ < section_count) {
        ui::section("Input");
        static float speed = 1.0f;
        ui::item("Move Speed", [&] {
            ImGui::DragFloat("##speed", &speed, 0.05f, 0.1f, 20.0f);
        });
        static int steps = 8;
        ui::item("Steps", [&] { ImGui::SliderInt("##steps", &steps, 1, 32); });
        static int flags = 0;
        ui::item("Flags", [&] {
            ImGui::BeginGroup();
            ImGui::CheckboxFlags("Visible", &flags, 1 << 0);
            ImGui::SameLine();
            ImGui::CheckboxFlags("Locked", &flags, 1 << 1);
            ImGui::EndGroup();
        });
        static char search[128] = "";
        ui::item("Filter", [&] {
            ImGui::InputTextWithHint("##filter", "Search...", search,
                                     sizeof(search));
        });
        static int direction = 0;
        ui::item("Direction", [&] {
            ImGui::BeginGroup();
            ImGui::RadioButton("X", &direction, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Y", &direction, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Z", &direction, 2);
            ImGui::EndGroup();
        });
    }
    if (count++ < section_count) {
        ui::section("Actions");
        ui::item(
            "Buttons",
            [&] {
                ImGui::BeginGroup();
                ImGui::Button("Apply");
                ImGui::SameLine();
                help_marker("This will apply the change");
                ImGui::EndGroup();
            },
            [&] {
                ImGui::BeginGroup();
                ImGui::Button("Reset");
                ImGui::SameLine();
                help_marker("This will reset the value");
                ImGui::EndGroup();
            });
        static bool confirmed = false;
        ui::item("Confirm", [&] {
            ImGui::BeginGroup();
            ImGui::Checkbox("##confirm", &confirmed);
            ImGui::SameLine();
            help_marker("I confirm");
            ImGui::EndGroup();
        });
        ui::item("Status", [&] {
            ImGui::TextColored(
                confirmed ? ImVec4(0.2f, 0.8f, 0.3f, 1.0f)
                          : ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                "%s", confirmed ? "Ready" : "Waiting for confirmation");
        });
    }
    if (count++ < section_count) {
        ui::section("Miscellaneous");
        ui::item("Progress", [&] {
            static float progress = 0.65f;
            float width = ImGui::CalcItemWidth();
            ImGui::ProgressBar(progress, ImVec2(width, 0.0f), "65%");
        });
        ui::item("Tooltip", [&] {
            ImGui::TextUnformatted("Hover me");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Example tooltip for layout testing");
            }
        });
        ui::item(
            "Long Label for Layout Testing", [&] { ImGui::Button("Short"); },
            [&] {
                ImGui::TextDisabled("This is some additional information");
            });
    }
    if (count++ < section_count) {
        ui::section();
        ui::item("Standalone Button", [&] { ImGui::Button("My Button"); });
    }
    ui::end_properties();
}

static void display_properties_test(bool* open) {
    if (!ImGui::Begin("Properties Test", open)) {
        ImGui::End();
        return;
    }
    draw_test_properties("my-properties");
    ui::begin_properties("advanced");
    static float range[2] = {0.2f, 0.8f};
    ui::item("Range",
             [&] { ImGui::SliderFloat2("##range", range, 0.0f, 1.0f); });
    static int selected = 1;
    ui::item("Priority", [&] {
        char const* priorities[] = {"Low", "Normal", "High", "Critical"};
        ImGui::Combo("##priority", &selected, priorities,
                     IM_ARRAYSIZE(priorities));
    });
    static bool enabled = true;
    static float threshold = 0.5f;
    ui::item(
        "Conditional", [&] { ImGui::Checkbox("Enabled", &enabled); },
        [&] {
            ImGui::BeginDisabled(!enabled);
            ImGui::SliderFloat("##threshold", &threshold, 0.0f, 1.0f);
            ImGui::EndDisabled();
        });
    static char description[256] = "A longer description for testing layout.";
    ui::item("Description", [&] {
        ImGui::InputTextMultiline(
            "##description", description, sizeof(description),
            ImVec2(0.0f, ImGui::GetTextLineHeight() * 3.0f));
    });
    ui::end_properties();

    ImGui::End();
}

void display_icon_viewer(bool* open) {
    ImGui::Begin("Icon Viewer", open);
    auto const& all_icons =
        get_current_ui_context()->font_manager.get_all_icon_codes();
    constexpr int columns = 6;
    constexpr float cell_height = 72.0f;
    if (ImGui::BeginTable(
            "Icons", columns,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) {
        for (auto const& [name, code] : all_icons) {
            ImGui::TableNextColumn();
            auto icon_data = ui::icon_data(name);
            float cell_width = ImGui::GetContentRegionAvail().x;
            float start_x = ImGui::GetCursorPosX();
            // Icon
            float icon_width = 24.0f;
            ImGui::SetCursorPosX(start_x + (cell_width - icon_width) * 0.5f);
            ui::with_font(icon_data.font, 32.0f,
                          [&] { ImGui::TextEx(icon_data.utf8_rep.data()); });
            // Name
            ImGui::SetCursorPosX(start_x);
            ImGui::SetNextItemWidth(cell_width);
            ImGui::TextWrapped("%s", name.c_str());
            // Give each cell a consistent minimum height.
            float remaining =
                cell_height - (ImGui::GetCursorPosY() - ImGui::GetCursorPosY());
            (void)remaining;
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

static std::string long_text() {
    return R"( Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur neque leo, ultricies in quam vel, bibendum tristique nisi. Pellentesque viverra ullamcorper dui sit amet lobortis. Nulla quis arcu lacus. Duis placerat id felis eu gravida. In et elit egestas, efficitur elit ac, dictum enim. Quisque hendrerit ac dui eu ullamcorper. Ut velit lectus, vulputate sit amet vestibulum quis, scelerisque at nisi. Integer vitae mi sapien. Duis tincidunt, purus vitae fringilla consectetur, lorem ligula mattis augue, vel accumsan tortor eros ut eros. Integer libero erat, bibendum ut efficitur quis, scelerisque vitae odio. Mauris tristique lacus a maximus congue. Integer posuere, urna efficitur tempus facilisis, elit ligula aliquet nulla, ut ornare lorem turpis at ante.
In congue felis in nisl mattis ornare. Suspendisse convallis sem quis sem consectetur elementum. Morbi lacinia ligula lectus, eget pulvinar odio maximus eu. Interdum et malesuada fames ac ante ipsum primis in faucibus. Fusce auctor velit ex, in volutpat nulla mattis et. Donec cursus nibh eu magna euismod accumsan. Nam sem nunc, posuere nec lacus nec, egestas gravida dui.
Aliquam scelerisque lacus ac nisl volutpat semper. Sed a nibh ac lorem placerat porta eu in tellus. Pellentesque aliquet diam at libero sodales, at bibendum leo tincidunt. Donec eu lobortis erat. Morbi facilisis orci erat, ultricies pharetra lorem consectetur eget. Phasellus tincidunt egestas condimentum. Mauris vel ligula eu odio mattis bibendum. Aenean tincidunt sem massa, a tincidunt magna mattis vitae. Phasellus sapien leo, gravida eu placerat pretium, volutpat vel tortor. Curabitur metus libero, ornare ac sagittis sed, imperdiet id justo. Duis et mattis orci. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae;
Duis mauris mi, dignissim in tincidunt vitae, egestas eget lorem. Praesent congue erat vel nulla dapibus fringilla. Cras vitae lobortis sem. Aenean semper nec nunc id ornare. Phasellus a sapien vitae justo egestas scelerisque eu non purus. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam hendrerit ultrices diam vel volutpat. Maecenas sit amet lacinia dui.
Etiam id eros at mauris sodales blandit non vulputate felis. Vivamus risus felis, scelerisque in nisi non, porta sodales odio. Nunc scelerisque laoreet eros ac rutrum. Donec interdum, elit eget fermentum sollicitudin, purus eros mollis neque, a pellentesque turpis massa non odio. Nullam varius dui tortor, eget varius augue malesuada vitae. Phasellus rutrum ligula purus, eu sodales augue molestie faucibus. Morbi mattis, ipsum vitae ultricies blandit, leo urna ultrices nibh, id sagittis felis orci ac velit. Vivamus mattis consequat nisl nec laoreet. Suspendisse a mollis dui. Fusce vitae sollicitudin odio, sed aliquam quam. Ut suscipit enim et mauris hendrerit, id lobortis nulla consequat. Praesent ac turpis eget ex tincidunt dapibus. Aliquam nec urna id leo tristique pellentesque non quis velit. Vestibulum finibus risus mattis efficitur pharetra. Quisque vel ullamcorper urna, id faucibus urna. )";
}

static ui::Ref<ui::SliderView<float>> make_float_slider() {
    auto value = std::make_shared<float>(30.0f);
    auto slider = ui::slider<float>({}, 0.0f, 300.0f);
    slider->set_state(
        {[value] { return *value; },
         [value, weak_slider = std::weak_ptr(slider)](float new_value) {
             *value = new_value;
             if (auto slider = weak_slider.lock())
                 slider |= ui::fixed_height(*value);
         }});
    return slider;
}

static void display_view_test(bool* open) {
    if (!ImGui::Begin("View Test", open)) {
        ImGui::End();
        return;
    }

    static int counter = 0;
    static bool button_active = true;
    static vml::float4 color;

    auto counter_text = [] {
        return ui::text([] { return std::to_string(counter); });
    };

    // clang-format off
    static ui::Ref<ui::View> view = ui::vscroll({
        ui::hstack({
            ui::vstack({
                counter_text(),
                ui::hseparator(),
                ui::hstack({
                    ui::spacer(2),
                    counter_text() | ui::align_top,
                    ui::spacer(),
                    counter_text() | ui::center,
                    ui::spacer(),
                    counter_text() | ui::align_bottom,
                    ui::spacer(2),
                }) | ui::hfill | ui::fixed_height(100),
                ui::hseparator(),
                ui::hstack({
                    counter_text() | ui::align_top,
                    counter_text() | ui::vcenter,
                    counter_text() | ui::align_bottom
                }) | ui::vfill,
                ui::hseparator(),
                ui::button("Count", [] { if (button_active) ++counter; })
            }) | ui::hfill,
            ui::vseparator(),
            ui::vstack({
                make_float_slider(),
                ui::slider<vml::float4>(&color, 0.0f, 1.0f, { .format = "%.3f" }),
                ui::text(long_text(), {.wrap = true}) | ui::vfill | ui::hfill
            }) | ui::vfill,
            ui::vseparator(),
            ui::vstack({
                ui::button("Yet another button"),
                ui::hseparator() | ui::fixed_height(50),
                ui::button("Yet another one") | ui::hfill, 
            }),
            ui::vseparator(),
            ui::button("...") 
        }) | ui::hfill,
        ui::hseparator(),
        ui::hstack({
            ui::spacer(1),
            ui::vseparator(),
            ui::vstack({
                ui::checkbox(&button_active) | ui::align_left, 
                ui::button("One More Button") | ui::center, 
                ui::button("??") | ui::align_right, 
            }) | ui::hfill_weight(5),
            ui::vseparator(),
            ui::spacer(1)
        }) | ui::hfill,
        ui::hseparator(),
        ui::button("Toggle Align", [] {}),
        ui::hseparator(),
        []{
            ui::ViewContainer<> children;
            for (int i = 0; i < 100; ++i) 
                children.push_back(ui::button(std::to_string(i)) | ui::vfill);
            return ui::hscroll(std::move(children)) | ui::fixed_height(40);
        }()
    });
    
    static ui::Ref<ui::TabView> tab_view = ui::tab_view({
        ui::tab("First Tab", view), 
        ui::tab("Second Tab", ui::vstack({
            ui::button("Other Side") | ui::center
        }) | ui::hfill | ui::vfill)
    });
    // clang-format on

    vml::float2 pos = ImGui::GetCursorScreenPos();
    vml::float2 size = ImGui::GetContentRegionAvail();
    tab_view->display({pos, size});

    ImGui::End();
}