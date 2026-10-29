#pragma once

#include <any>
#include <cassert>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include <utl/hashtable.hpp>

#include <mosaic/support/memory.h>
#include <mosaic/tool/tool_class.h>

namespace mosaic {

class Tool;

template <typename... Setters> using ToolAssocData = std::tuple<Setters...>;

struct ToolRegistryEntry {
    std::function<Owner<Tool>()> factory;
    std::function<void(Tool&, std::any)> data_setter;
};

class ToolRegistry {
public:
    template <typename ToolType, typename ToolId = ToolType::ToolId,
              typename... Setters>
    void register_tool(std::function<Owner<Tool>()> factory,
                       ToolAssocData<Setters...> actions = {});

    ToolRegistryEntry* find_entry(ToolClass const* tool_class);

private:
    utl::hashmap<ToolClass const*, std::unique_ptr<ToolRegistryEntry>>
        m_entries;
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
        auto data_setter = [data_setters](Tool& tool, std::any data) {
            auto* derived = dynamic_cast<ToolType*>(&tool);
            assert(derived);
            bool success =
                (... || internal::try_set_tool_data(
                            *derived, std::get<I>(data_setters), data));
            assert(success);
        };
        auto entry = allocate<ToolRegistryEntry>(
            ToolRegistryEntry{std::move(factory), std::move(data_setter)});
        ToolClass const* tool_class = &ToolId::tool_class;
        auto [itr, success] = m_entries.emplace(tool_class, std::move(entry));
        assert(success);
    }(std::make_index_sequence<num_actions>{});
}