#include "tool/demo_tool.h"

#include <utl/common.hpp>

#include "tool/tool.h"
#include "tool/tool_registry.h"

using namespace mosaic;

namespace {

struct DemoTool : Tool {
    using ToolId = DemoToolId;

    void update() override {}

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