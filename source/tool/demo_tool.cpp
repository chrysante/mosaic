#include "tool/demo_tool.h"

#include <utl/common.hpp>

#include "support/json_support.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"

using namespace mosaic;

namespace {

struct DemoTool : Tool {
    using ToolId = DemoToolId;

    DemoTool() {}

    void update() override {}

    nlohmann::json save_config() const override;
    void load_config(nlohmann::json const&) override;

    std::string message;
};

} // namespace

UTL_STATIC_INIT {
    get_global_tool_registry().register_tool<DemoTool>(
        [] { return allocate<DemoTool>(); },
        ToolAssocData([](DemoTool& tool, DemoToolOpenData open_data) {
            tool.message = std::move(open_data).message;
        }));
};

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
