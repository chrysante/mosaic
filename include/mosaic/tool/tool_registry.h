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

template <typename... Setters> using ToolAssocData = std::tuple<Setters...>;

class ToolRegistryEntry {
public:
    virtual ~ToolRegistryEntry() = default;

    virtual ToolClass const* tool_class() const = 0;

    virtual Owner<Tool> create(std::uint64_t instance_id,
                               ServiceContext& service_context) const = 0;

    virtual void set_data(Tool& tool, std::any data) const = 0;
};

class ToolRegistry {
public:
    template <typename ToolType, typename ToolId = ToolType::ToolId,
              typename... Setters>
    void register_tool(std::function<Owner<Tool>()> factory,
                       ToolAssocData<Setters...> actions = {});

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

namespace mosaic::internal {

template <typename F> struct ToolDataTraitsImpl;

template <typename R, typename ToolTypeParam, typename DataTypeParam>
struct ToolDataTraitsImpl<std::function<R(ToolTypeParam, DataTypeParam)>> {
    using ToolType = std::remove_cvref_t<ToolTypeParam>;
    using DataType = std::remove_cvref_t<DataTypeParam>;
};

template <typename Action>
struct ToolDataTraits
    : ToolDataTraitsImpl<std::remove_reference_t<decltype(std::function{
          std::declval<Action>()})>> {};

template <typename Action, typename ToolType>
bool try_set_tool_data(ToolType& tool, Action& action, std::any& data) {
    using DataType = internal::ToolDataTraits<Action>::DataType;
    auto* concrete = std::any_cast<DataType>(&data);
    if (!concrete) return false;
    std::invoke(action, tool, std::move(*concrete));
    return true;
}
} // namespace mosaic::internal

template <typename ToolType, typename ToolId, typename... Setters>
void mosaic::ToolRegistry::register_tool(
    std::function<Owner<Tool>()> factory,
    ToolAssocData<Setters...> data_setters) {
    static constexpr std::size_t num_actions = sizeof...(Setters);
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        struct RegEntryImpl final : ToolRegistryEntry {
            explicit RegEntryImpl(std::function<Owner<Tool>()> factory,
                                  ToolAssocData<Setters...> data_setters)
                : factory(std::move(factory))
                , data_setters(std::move(data_setters)) {}

            ToolClass const* tool_class() const override {
                return &ToolId::tool_class;
            }

            Owner<Tool> create(std::uint64_t instance_id,
                               ServiceContext& service_context) const override {
                auto tool = factory();
                tool->m_tool_id = {&ToolId::tool_class, instance_id};
                tool->m_service_context = &service_context;
                return tool;
            }

            void set_data(Tool& tool, std::any data) const override {
                auto* derived = dynamic_cast<ToolType*>(&tool);
                assert(derived);
                bool success =
                    (... || internal::try_set_tool_data(
                                *derived, std::get<I>(data_setters), data));
                assert(success);
            }

            std::function<Owner<Tool>()> factory;
            ToolAssocData<Setters...> data_setters;
        };

        auto entry =
            allocate<RegEntryImpl>(std::move(factory), std::move(data_setters));
        ToolClass const* tool_class = &ToolId::tool_class;
        bool success = m_entries.emplace(tool_class, std::move(entry)).second;
        success &= m_name_map.emplace(tool_class->name, tool_class).second;
        assert(success);
    }(std::make_index_sequence<num_actions>{});
}