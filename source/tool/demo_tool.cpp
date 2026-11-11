#include "tool/demo_tool.h"

#include <utl/common.hpp>

#include "gui/containers.h"
#include "gui/icon.h"
#include "gui/panel.h"
#include "gui/property_editor.h"
#include "gui/toolbar.h"
#include "gui/view.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "service/panel_manager.h"
#include "service/service_context.h"
#include "support/json_support.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"

using namespace mosaic;

namespace {

struct DemoTool : Tool {
    static ToolClass const tool_class;

    void on_create() override;

    Owner<ui::View> make_toolbar();
    Owner<ui::View> make_view();

    void update() override;

    nlohmann::json save_config() const override;
    void load_config(nlohmann::json const&) override;

    Handle<Panel> panel;
    std::string message;
    bool allow_close = true;
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

void DemoTool::on_create() {
    panel = service_context().panel_manager().create_panel(
        {.title = "Demo Tool"}, std::make_unique<DemoToolPanelDelegate>(this));
    panel->set_view(make_view());
}

Owner<ui::View> DemoTool::make_toolbar() {
    return ui::toolbar({ui::button(ui::lucide_icon("play"))});
}

Owner<ui::View> DemoTool::make_view() {
    auto using_toolbar =
        ui::computed([this] { return panel->toolbar() != nullptr; },
                     [this](bool new_value) {
                         if (new_value) panel->set_toolbar(make_toolbar());
                     });
    // clang-format off
    return ui::vscroll({
        ui::section("Misc", {
            ui::property("", ui::button("Kill", [this] { request_kill(); }) |
                             ui::tooltip("Immediately kill the tool")),
            ui::property("Allow Close", ui::checkbox(ui::bind(&allow_close))),
            ui::property("Use Toolbar", ui::checkbox(std::move(using_toolbar))),
        })
    });
    // clang-format on
}

void DemoTool::update() {}

namespace config_key { static constexpr std::string_view string = "string"; }

nlohmann::json DemoTool::save_config() const {
    auto object = nlohmann::json::object();
    object[config_key::string] = message;
    return object;
}

void DemoTool::load_config(nlohmann::json const& object) {
    message = json_find<std::string>(object, config_key::string)
                  .value_or("Empty config string");
}

ApplicationCommand mosaic::demo_tool_open_command(ToolOpenPolicy policy,
                                                  DemoToolOpenData open_data) {
    return make_tool_open_command<DemoTool>(
        policy, [open_data = std::move(open_data)](DemoTool& demo_tool) {
            demo_tool.message = std::move(open_data).message;
        });
}