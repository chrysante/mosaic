#pragma once

#include <any>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include <utl/hashtable.hpp>

#include <mosaic/support/memory.h>
#include <mosaic/tool/tool_class.h>

namespace mosaic {

class ServiceContext;
class Tool;

class ToolRegistryEntry {
public:
    virtual ~ToolRegistryEntry() = default;

    virtual ToolClass const* tool_class() const = 0;

    virtual Owner<Tool> create(std::uint64_t instance_id,
                               ServiceContext& service_context) const = 0;
};

class ToolRegistry {
public:
    template <typename ToolType>
    void register_tool(std::function<Owner<Tool>()> factory);

    ToolRegistryEntry const* find_entry(ToolClass const* tool_class) const;

    ToolRegistryEntry const* find_entry(std::string name) const;

private:
    utl::hashmap<ToolClass const*, std::unique_ptr<ToolRegistryEntry>>
        m_entries;
    utl::hashmap<std::string, ToolClass const*> m_name_map;
};

ToolRegistry& get_global_tool_registry();

} // namespace mosaic

// =============================================================================
// Inline implementation
// =============================================================================

template <typename ToolType>
void mosaic::ToolRegistry::register_tool(std::function<Owner<Tool>()> factory) {
    struct RegEntryImpl final : ToolRegistryEntry {
        explicit RegEntryImpl(std::function<Owner<Tool>()> factory)
            : factory(std::move(factory)) {}

        ToolClass const* tool_class() const override {
            return &ToolType::tool_class;
        }

        Owner<Tool> create(std::uint64_t instance_id,
                           ServiceContext& service_context) const override {
            auto tool = factory();
            tool->m_tool_id = {&ToolType::tool_class, instance_id};
            tool->m_service_context = &service_context;
            return tool;
        }

        std::function<Owner<Tool>()> factory;
    };

    auto entry = allocate<RegEntryImpl>(std::move(factory));
    ToolClass const* tool_class = &ToolType::tool_class;
    bool success = m_entries.emplace(tool_class, std::move(entry)).second;
    success &= m_name_map.emplace(tool_class->name, tool_class).second;
    assert(success);
}