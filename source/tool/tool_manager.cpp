#include "tool/tool_manager.h"

#include <concepts>

#include <nlohmann/json.hpp>
#include <utl/hashtable.hpp>

#include "support/memory.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"
#include "utl/scope_guard.hpp"

using namespace mosaic;

struct internal::ToolManagerImpl {
    void for_each_tool(std::invocable<Tool&> auto&& callback) {
        for (auto& [tool_class, list] : tools)
            for (auto& tool : list)
                callback(*tool);
    }

    Tool* find_open_tool(ToolClass const* tool_class);

    Tool* open_new_tool(ToolClass const* tool_class);

    utl::hashmap<ToolClass const*, std::vector<Owner<Tool>>> tools;
    bool in_update = false;
};

using internal::ToolManagerImpl;

ToolManager::ToolManager()
    : m_impl(std::make_unique<ToolManagerImpl>()) {}

ToolManager::~ToolManager() = default;

Tool* ToolManagerImpl::find_open_tool(ToolClass const* tool_class) {
    auto itr = tools.find(tool_class);
    if (itr == tools.end()) return nullptr;
    auto& list = itr->second;
    return list.empty() ? nullptr : list.front().get();
}

Tool* ToolManagerImpl::open_new_tool(ToolClass const* tool_class) {
    auto& registry = get_global_tool_registry();
    auto* entry = registry.find_entry(tool_class);
    if (!entry) return nullptr;
    auto& list = tools[tool_class];
    auto tool_owner = entry->factory();
    auto* tool = tool_owner.get();
    list.push_back(std::move(tool_owner));
    return tool;
}

void ToolManager::open_tool(ToolClass const* tool_class, ToolOpenPolicy policy,
                            std::any data) {
    assert(!m_impl->in_update &&
           "Must not call ToolManager::open_tool() in the update loop");
    auto* tool = [&] {
        switch (policy) {
        case ToolOpenPolicy::open:
            return m_impl->open_new_tool(tool_class);
        case ToolOpenPolicy::open_or_find:
            if (auto* result = m_impl->find_open_tool(tool_class))
                return result;
            return m_impl->open_new_tool(tool_class);
        }
    }();
    if (tool && data.has_value()) {
        auto& registry = get_global_tool_registry();
        auto* entry = registry.find_entry(tool_class);
        if (entry) entry->data_setter(*tool, std::move(data));
    }
}

void ToolManager::update() {
    m_impl->in_update = true;
    utl::scope_guard update_end = [&] { m_impl->in_update = false; };
    m_impl->for_each_tool([](Tool& tool) { tool.update(); });
}

nlohmann::json ToolManager::save_settings() const {
    return nlohmann::json::object();
}

void ToolManager::load_settings(nlohmann::json const& object) {}