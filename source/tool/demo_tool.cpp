#include "tool/demo_tool.h"

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <utl/common.hpp>

#include "application/input_event.h"
#include "editor/node_editor.h"
#include "gui/containers.h"
#include "gui/icon.h"
#include "gui/panel.h"
#include "gui/property_editor.h"
#include "gui/toolbar.h"
#include "gui/view.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "service/event_receiver.h"
#include "service/panel_manager.h"
#include "service/service_context.h"
#include "support/json_support.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"

using namespace mosaic;

namespace {

struct DemoTool : Tool {
    static ToolClass const tool_class;

    DemoTool();

    void on_create() override;

    Owner<ui::View> make_toolbar();
    Owner<ui::View> make_view();

    void update() override;

    nlohmann::json save_config() const override;
    void load_config(nlohmann::json const&) override;

    Handle<Panel> panel;
    bool allow_close = true;
    NodeEditorGraph node_graph;
    NodeEditor node_editor;
    EventReceiver event_receiver;
};

struct DemoToolPanelDelegate : PanelDelegate {
    DemoTool* self;

    explicit DemoToolPanelDelegate(DemoTool* self)
        : self(self) {}

    bool on_close_request() override { return self->allow_close; }

    void on_destroy() override { self->request_kill(); }
};

} // namespace

ToolClass const DemoTool::tool_class = {.name = "DemoTool"};

UTL_STATIC_INIT {
    get_global_tool_registry().register_tool<DemoTool>(
        [] { return allocate<DemoTool>(); });
};

namespace {

struct NodeEditorView : ui::View {
    explicit NodeEditorView(NodeEditor* node_editor)
        : View({ui::SizeSpec::fill(), ui::SizeSpec::fill()})
        , node_editor(node_editor) {}

    virtual void do_display(Rect frame) {
        ImGui::SetNextWindowPos(frame.origin);
        bool visible = ImGui::BeginChild("NodeEditorChild", frame.size);
        if (visible) node_editor->display();
        ImGui::EndChild();
    }

    virtual vml::float2 do_calc_intrinsic_size(vml::float2 max_size) const {
        return max_size;
    }

    NodeEditor* node_editor;
};

static Owner<ui::View> make_node_editor_view(NodeEditor* node_editor) {
    return allocate<NodeEditorView>(node_editor);
}

struct DemoNodeController : node_editor::NodeController {
    std::vector<node_editor::PinDesc> make_input_descriptors() override {
        return {node_editor::PinDesc{"A"}, node_editor::PinDesc{"B"}};
    }

    std::vector<node_editor::PinDesc> make_output_descriptors() override {
        return {node_editor::PinDesc{"Output"}};
    }

    vml::float2 preferred_content_size() const override {
        return {200.0f, 100.0f};
    }
};

struct DemoNodeEditorDelegate : NodeEditorDelegate {
    explicit DemoNodeEditorDelegate(NodeEditorGraph& graph)
        : graph(graph) {}

    void background_context_menu() override {}

    node_editor::Node* create_node_popup_menu() override {
        if (ImGui::MenuItem("Create Node"))
            return graph.add_node(node_editor::NodeDesc{
                .name = "My Node",
                .resizable = true,
                .controller = std::make_unique<DemoNodeController>()});
        return nullptr;
    }

    std::unique_ptr<node_editor::NodeController>
    deserialize_node_controller(nlohmann::json const&) override {
        return std::make_unique<DemoNodeController>();
    }

    NodeEditorGraph& graph;
};

} // namespace

DemoTool::DemoTool()
    : node_editor(command_palette_write()) {}

void DemoTool::on_create() {
    panel = service_context().panel_manager().create_panel(
        to_panel_id(id()), {.title = "Demo Tool"},
        std::make_unique<DemoToolPanelDelegate>(this));
    panel->set_view(make_view());
    node_editor.set_model(&node_graph);
    event_receiver = service_context().make_event_receiver();
    event_receiver.listen([this](InputEventUnion const& event) {
        node_editor.on_input(event.base());
    });
    node_editor.set_delegate(
        std::make_unique<DemoNodeEditorDelegate>(node_graph));
}

Owner<ui::View> DemoTool::make_toolbar() {
    return ui::toolbar({ui::button(ui::lucide_icon("play"))});
}

Owner<ui::View> DemoTool::make_view() {
    auto using_toolbar = ui::computed(
        [this] { return panel->toolbar() != nullptr; },
        [this](bool v) { panel->set_toolbar(v ? make_toolbar() : nullptr); });
    auto height_selection =
        ui::computed([this] { return (int)panel->style().toolbar_height; },
                     [this](int new_value) {
                         if (new_value < 0 || new_value > 3) return;
                         auto style = panel->style();
                         style.toolbar_height = (PanelToolbarHeight)new_value;
                         panel->set_style(style);
                     });
    auto height_preview = ui::computed([this] {
        return (std::string)magic_enum::enum_name(
            panel->style().toolbar_height);
    });
    auto height_items =
        ui::own(std::vector<std::string>{"Small", "Medium", "Large"});
    // clang-format off
    auto settings = ui::vscroll({
        ui::section("Misc", {
            ui::property("", ui::button("Kill", [this] { request_kill(); }) |
                             ui::tooltip("Immediately kill the tool")),
            ui::property("Allow Close", ui::checkbox(ui::bind(&allow_close))),
            ui::property("Use Toolbar", ui::checkbox(std::move(using_toolbar))),
            ui::property("Toolbar Height", ui::combo(std::move(height_selection), 
                                                     std::move(height_preview),
                                                     std::move(height_items))),
        })
    });
    // clang-format on
    auto node_editor_view = make_node_editor_view(&node_editor);
    return ui::tab_view({ui::tab("Settings", std::move(settings)),
                         ui::tab("Nodes", std::move(node_editor_view))});
}

void DemoTool::update() {}

namespace config_key {
static constexpr std::string_view node_editor = "node_editor";
static constexpr std::string_view node_graph = "node_graph";
} // namespace config_key

nlohmann::json DemoTool::save_config() const {
    auto object = nlohmann::json::object();
    object[config_key::node_editor] = node_editor.serialize();
    object[config_key::node_graph] = node_graph.serialize();
    return object;
}

void DemoTool::load_config(nlohmann::json const& object) {
    if (auto* config = json_find(object, config_key::node_editor))
        node_editor.deserialize(*config);
    if (auto* document = json_find(object, config_key::node_graph))
        node_graph.deserialize(*document, [](nlohmann::json const&) {
            return std::make_unique<DemoNodeController>();
        });
}

ApplicationCommand mosaic::demo_tool_open_command(ToolOpenPolicy policy,
                                                  DemoToolOpenData open_data) {
    return make_tool_open_command<DemoTool>(
        policy, [open_data = std::move(open_data)](DemoTool&) {});
}