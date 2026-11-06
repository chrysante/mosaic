#pragma once

#include <cstdint>
#include <string>

namespace mosaic {

struct ToolClass { std::string name; };

struct ToolId {
    ToolClass const* tool_class = nullptr;
    std::uint64_t instance = -1;
};

}; // namespace mosaic