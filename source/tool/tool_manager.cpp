#include "tool/tool_manager.h"

#include <concepts>
#include <vector>

#include <nlohmann/json.hpp>
#include <utl/hashtable.hpp>
#include <utl/scope_guard.hpp>

#ifndef NDEBUG
#include <imgui.h>
#endif

#include "service/service_context.h"
#include "support/json_support.h"
#include "support/logger.h"
#include "support/memory.h"
#include "tool/tool.h"
#include "tool/tool_registry.h"

using namespace mosaic;

using ToolMap = utl::hashmap<std::uint64_t, Owner<Tool>>;

struct ToolManager::Impl {
    Impl(ServiceContext& service_context)
        : service_context(service_context) {}

    void for_each_tool(std::invocable<ToolClass const*, std::uint64_t,
                                      Tool&> auto&& callback) {
        for (auto& [tool_class, map] : tools)
            for (auto& [instance, tool] : map)
                std::invoke(callback, tool_class, instance, *tool);
    }

    void for_each_tool(std::invocable<Tool&> auto&& callback) {
        for_each_tool([&](ToolClass const*, std::uint64_t, Tool& tool) {
            std::invoke(callback, tool);
        });
    }

    std::uint64_t new_instance_id(ToolClass const* tool_class) const;
    Tool* insert_tool(ToolId tool_id, Owner<Tool> tool);
    Tool* find_open_tool(ToolClass const* tool_class);
    Tool* open_new_tool(ToolClass const* tool_class);
    Tool* find_by_id(ToolId tool_id);
    Tool* create_tool(ToolRegistryEntry const& entry,
                      std::uint64_t instance_id);
    void kill_tool(ToolClass const*, std::uint64_t instance_id);
    void defer(std::function<void()> operation);
    void process_deferred_operations();

    ServiceContext& service_context;
    utl::hashmap<ToolClass const*, ToolMap> tools;
    bool in_update = false;
    std::vector<std::function<void()>> deferred_operations, def_op_back_buffer;
};

ToolManager::ToolManager(ServiceContext& service_context)
    : m_impl(std::make_unique<Impl>(service_context)) {}

ToolManager::~ToolManager() = default;

std::uint64_t
ToolManager::Impl::new_instance_id(ToolClass const* tool_class) const {
    auto map_itr = tools.find(tool_class);
    if (map_itr == tools.end()) return 0;
    auto& map = map_itr->second;
    std::uint64_t id = 0;
    for (;; ++id)
        if (!map.contains(id)) break;
    return id;
}

Tool* ToolManager::Impl::insert_tool(ToolId tool_id, Owner<Tool> tool) {
    assert(tool != nullptr);
    auto* result = tool.get();
    auto [_, success] =
        tools[tool_id.tool_class].emplace(tool_id.instance, std::move(tool));
    assert(success);
    return result;
}

Tool* ToolManager::Impl::find_open_tool(ToolClass const* tool_class) {
    auto itr = tools.find(tool_class);
    if (itr == tools.end()) return nullptr;
    auto& list = itr->second;
    return list.empty() ? nullptr : list.begin()->second.get();
}

Tool* ToolManager::Impl::open_new_tool(ToolClass const* tool_class) {
    auto& registry = get_global_tool_registry();
    auto* entry = registry.find_entry(tool_class);
    if (!entry) {
        Logger::error("Cannot open tool: does not exist");
        return nullptr;
    }
    Tool* tool = create_tool(*entry, new_instance_id(tool_class));
    tool->on_create();
    return tool;
}

Tool* ToolManager::Impl::find_by_id(ToolId tool_id) {
    auto map_itr = tools.find(tool_id.tool_class);
    if (map_itr == tools.end()) return nullptr;
    auto& map = map_itr->second;
    auto itr = map.find(tool_id.instance);
    return itr != map.end() ? itr->second.get() : nullptr;
}

Tool* ToolManager::Impl::create_tool(ToolRegistryEntry const& entry,
                                     std::uint64_t instance_id) {
    auto tool = entry.create(instance_id, service_context);
    auto tool_id = tool->tool_id();
    return insert_tool(tool_id, std::move(tool));
}

void ToolManager::Impl::kill_tool(ToolClass const* tool_class,
                                  std::uint64_t instance_id) {
    auto map_itr = tools.find(tool_class);
    assert(map_itr != tools.end());
    auto& map = map_itr->second;
    bool success = map.erase(instance_id) == 1;
    assert(success);
    if (map.empty()) tools.erase(map_itr);
}

void ToolManager::Impl::defer(std::function<void()> operation) {
    deferred_operations.push_back(std::move(operation));
}

void ToolManager::Impl::process_deferred_operations() {
    std::swap(deferred_operations, def_op_back_buffer);
    for (auto& operation : def_op_back_buffer)
        operation();
    def_op_back_buffer.clear();
}

Tool* ToolManager::open_tool(ToolClass const* tool_class,
                             ToolOpenPolicy policy) {
    assert(!m_impl->in_update &&
           "Must not call ToolManager::open_tool() in the update loop");
    switch (policy) {
    case ToolOpenPolicy::open:
        return m_impl->open_new_tool(tool_class);
    case ToolOpenPolicy::open_or_find:
        if (auto* result = m_impl->find_open_tool(tool_class)) return result;
        return m_impl->open_new_tool(tool_class);
    }
}

void ToolManager::update() {
    m_impl->process_deferred_operations();
    m_impl->in_update = true;
    m_impl->for_each_tool(
        [&](ToolClass const* tool_class, std::uint64_t instance, Tool& tool) {
            tool.update();
            if (tool.m_kill_requested)
                m_impl->defer([tool_class, instance, this] {
                    m_impl->kill_tool(tool_class, instance);
                });
        });
    m_impl->in_update = false;
    m_impl->process_deferred_operations();
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
        Tool* tool = m_impl->create_tool(*registry_entry, tool_id.instance);
        if (auto* state = json_find(node, config_key::state))
            tool->load_config(*state);
        tool->on_create();
    }
}

#ifndef NDEBUG
void ToolManager::draw_debugger() {
    for (auto& [tool_class, map] : m_impl->tools) {
        if (!ImGui::TreeNodeEx(tool_class->name.c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen))
            continue;
        for (auto& [instance, tool] : map) {
            if (!tool) {
                ImGui::BulletText("[%u] NULL", (unsigned)instance);
                continue;
            }
            ImGui::PushID(static_cast<int>(instance));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("[%u]", (unsigned)instance);
            ImGui::SameLine();
            ImGui::TextDisabled("(%p)", static_cast<void const*>(tool.get()));
            ImGui::SameLine();
            if (ImGui::Button("Kill"))
                m_impl->defer(
                    [=, this] { m_impl->kill_tool(tool_class, instance); });
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}
#endif // NDEBUG
