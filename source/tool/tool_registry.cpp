#include "tool/tool_registry.h"

#include <cassert>

#include "support/logger.h"

using namespace mosaic;

ToolRegistry& mosaic::get_global_tool_registry() {
    static ToolRegistry instance;
    return instance;
}

ToolRegistryEntry const*
ToolRegistry::find_entry(ToolClass const* tool_class) const {
    auto itr = m_entries.find(tool_class);
    if (itr == m_entries.end()) {
        Logger::error("Failed to find tool class in registry");
        return nullptr;
    }
    auto* result = itr->second.get();
    assert(result);
    return result;
}

ToolRegistryEntry const* ToolRegistry::find_entry(std::string name) const {
    auto itr = m_name_map.find(name);
    if (itr == m_name_map.end()) return nullptr;
    return find_entry(itr->second);
}
