#pragma once

#include <string>

#include <mosaic/tool/tool_class.h>

namespace mosaic {

struct DemoToolId {
    static inline ToolClass const tool_class = {.name = "DemoTool"};
};

struct DemoToolOpenData {
    using ToolId = DemoToolId;

    std::string message;
};

} // namespace mosaic