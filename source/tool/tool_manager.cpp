#include "tool/tool_manager.h"

#include <concepts>

#include <nlohmann/json.hpp>
#include <utl/hashtable.hpp>

#include "service/service_context.h"
#include "support/json_support.h"
#include "support/logger.h"
#include "support/memory.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"
#include "utl/scope_guard.hpp"

using namespace mosaic;

using ToolMap = utl::hashmap<std::uint64_t, Owner<Tool>>;

struct internal::ToolManagerImpl {
    ToolManagerImpl(ServiceContext& service_context)
        : service_context(service_context) {}

    void for_each_tool(std::invocable<Tool&> auto&& callback) {
        for (auto& [tool_class, map] : tools)
            for (auto& [instance, tool] : map)
                callback(*tool);
    }

    std::uint64_t new_instance_id(ToolClass const* tool_class) const;
    Tool* insert_tool(ToolId tool_id, Owner<Tool> tool);
    Tool* find_open_tool(ToolClass const* tool_class);
    Tool* open_new_tool(ToolClass const* tool_class);
    Tool* find_by_id(ToolId tool_id);

    ServiceContext& service_context;
    utl::hashmap<ToolClass const*, ToolMap> tools;
    bool in_update = false;
};

using internal::ToolManagerImpl;

ToolManager::ToolManager(ServiceContext& service_context)
    : m_impl(std::make_unique<ToolManagerImpl>(service_context)) {}

ToolManager::~ToolManager() = default;

std::uint64_t
ToolManagerImpl::new_instance_id(ToolClass const* tool_class) const {
    auto map_itr = tools.find(tool_class);
    if (map_itr == tools.end()) return 0;
    auto& map = map_itr->second;
    std::uint64_t id = 0;
    for (;; ++id)
        if (!map.contains(id)) break;
    return id;
}

Tool* ToolManagerImpl::insert_tool(ToolId tool_id, Owner<Tool> tool) {
    assert(tool != nullptr);
    auto* result = tool.get();
    auto [_, success] =
        tools[tool_id.tool_class].emplace(tool_id.instance, std::move(tool));
    assert(success);
    return result;
}

Tool* ToolManagerImpl::find_open_tool(ToolClass const* tool_class) {
    auto itr = tools.find(tool_class);
    if (itr == tools.end()) return nullptr;
    auto& list = itr->second;
    return list.empty() ? nullptr : list.begin()->second.get();
}

Tool* ToolManagerImpl::open_new_tool(ToolClass const* tool_class) {
    auto& registry = get_global_tool_registry();
    auto* entry = registry.find_entry(tool_class);
    if (!entry) {
        Logger::error("Cannot open tool: does not exist");
        return nullptr;
    }
    std::uint64_t instance = new_instance_id(tool_class);
    auto tool = entry->create(instance, service_context);
    auto tool_id = tool->tool_id();
    return insert_tool(tool_id, std::move(tool));
}

Tool* ToolManagerImpl::find_by_id(ToolId tool_id) {
    auto map_itr = tools.find(tool_id.tool_class);
    if (map_itr == tools.end()) return nullptr;
    auto& map = map_itr->second;
    auto itr = map.find(tool_id.instance);
    return itr != map.end() ? itr->second.get() : nullptr;
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
        assert(entry);
        entry->set_data(*tool, std::move(data));
    }
}

void ToolManager::update() {
    m_impl->in_update = true;
    utl::scope_guard update_end = [&] { m_impl->in_update = false; };
    m_impl->for_each_tool([](Tool& tool) { tool.update(); });
}

static nlohmann::json serialize(ToolId const& tool_id) {
    nlohmann::json j = nlohmann::json::object();
    j["class"] = tool_id.tool_class->name;
    j["instance"] = tool_id.instance;
    return j;
}

namespace {
struct RestoredToolId {
    ToolId tool_id;
    ToolRegistryEntry const* registry_entry;
};
} // namespace

static std::optional<RestoredToolId> deserialize(nlohmann::json const& j,
                                                 ToolRegistry const& registry) {
    auto name = json_find<std::string>(j, "class");
    auto instance = json_find<std::uint64_t>(j, "instance");
    if (!name || !instance) return std::nullopt;
    auto* entry = registry.find_entry(*name);
    if (!entry) return std::nullopt;
    return RestoredToolId{{entry->tool_class(), *instance}, entry};
}

namespace config_key {

static constexpr std::string_view tool_id = "id";
static constexpr std::string_view state = "state";

} // namespace config_key

nlohmann::json ToolManager::save_config() const {
    auto root = nlohmann::json::array();
    m_impl->for_each_tool([&](Tool& tool) {
        auto object = nlohmann::json::object();
        object[config_key::tool_id] = serialize(tool.tool_id());
        object[config_key::state] = tool.save_config();
        root.push_back(std::move(object));
    });
    return root;
}

void ToolManager::load_config(nlohmann::json const& root) {
    m_impl->tools.clear();
    if (!root.is_array()) return;
    for (auto const& node : root) {
        if (!node.is_object()) {
            Logger::error("Failed to deserialize tool: not an object");
            continue;
        }
        auto* id_node = json_find(node, config_key::tool_id);
        if (!id_node) {
            Logger::error("Failed to deserialize tool: no ID");
            continue;
        }
        auto restored_tool_id =
            deserialize(*id_node, get_global_tool_registry());
        if (!restored_tool_id) {
            Logger::error("Failed to deserialize tool: invalid ID");
            continue;
        }
        auto [tool_id, registry_entry] = *restored_tool_id;
        if (m_impl->find_by_id(tool_id) != nullptr) {
            Logger::error(
                "Failed to deserialize tool: instance ID already taken");
            continue;
        }
        auto tool =
            registry_entry->create(tool_id.instance, m_impl->service_context);
        if (auto* state = json_find(node, config_key::state))
            tool->load_config(*state);
        m_impl->insert_tool(tool_id, std::move(tool));
    }
}