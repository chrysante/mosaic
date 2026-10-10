#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include <utl/common.hpp>
#include <utl/hashtable.hpp>

namespace mosaic {

class ServiceContext;
class View;

class ViewRegistryEntry {
public:
    using Factory = std::function<std::unique_ptr<View>()>;

    ViewRegistryEntry(std::string name, Factory factory)
        : m_name(std::move(name))
        , m_factory(std::move(factory)) {}

    std::string const& name() const { return m_name; }

    std::unique_ptr<View> create_view(ServiceContext&) const;

private:
    std::string m_name;
    Factory m_factory;

    friend class ViewRegistry;
};

class ViewRegistry {
public:
    static void add(ViewRegistryEntry entry);

    static std::optional<ViewRegistryEntry> get(std::string_view name);

    static auto entries() {
        return instance().m_entries.values() | std::views::values;
    }

private:
    ViewRegistry() = default;

    static ViewRegistry& instance();

    utl::hashmap<std::string, ViewRegistryEntry> m_entries;
};

#define MOSAIC_REGISTER_VIEW(ViewClass, Name)                                  \
    UTL_STATIC_INIT {                                                          \
        ::mosaic::ViewRegistry::add(                                           \
            {Name, [] { return std::make_unique<ViewClass>(); }});             \
    }

} // namespace mosaic
