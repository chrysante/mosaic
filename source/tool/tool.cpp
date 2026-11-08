#include "tool/tool.h"

#include <nlohmann/json.hpp>

#include "application/application.h"
#include "gui/panel.h" // IWYU pragma: keep
#include "tool/tool_class.h"
#include "tool/tool_manager.h"

using namespace mosaic;

Tool::Tool() = default;

Tool::~Tool() = default;

nlohmann::json Tool::save_config() const { return nlohmann::json::object(); }

Tool* mosaic::internal::create_tool(Application& app,
                                    ToolClass const* tool_class,
                                    ToolOpenPolicy policy) {
    return app.tool_manager().open_tool(tool_class, policy);
}
