#include "tool/tool.h"

#include <nlohmann/json.hpp>
#include <utl/hash.hpp>

#include "application/application.h"
#include "gui/panel.h" // IWYU pragma: keep
#include "tool/tool_class.h"
#include "tool/tool_manager.h"
#include "utl/hash.hpp"

using namespace mosaic;

Tool::Tool() = default;

Tool::~Tool() = default;

nlohmann::json Tool::save_config() const { return nlohmann::json::object(); }

int mosaic::to_panel_id(ToolId const& id) {
    std::uint64_t hash = utl::hash_combine(id.tool_class->name, id.instance);
    return static_cast<int>(static_cast<std::uint32_t>(hash) ^
                            static_cast<std::uint32_t>(hash << 32));
}

Tool* mosaic::internal::create_tool(Application& app,
                                    ToolClass const* tool_class,
                                    ToolOpenPolicy policy) {
    return app.tool_manager().open_tool(tool_class, policy);
}
