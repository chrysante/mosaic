#pragma once

#include <functional>
#include <memory>
#include <ranges>
#include <string>

#include <utl/common.hpp>
#include <utl/hashtable.hpp>

/// Registers the view class \p ViewClass with the registry
#define MOSAIC_REGISTER_VIEW(ViewClass, Name)                                  \
    UTL_STATIC_INIT {                                                          \
        ViewRegistry::add(                                                     \
            {Name, [] { return std::make_unique<ViewClass>(); }});             \
    }

namespace mosaic {

class ServiceContext;
class View;

/// Entry in the view manager that represents a concrete view class
class ViewRegistryEntry {
public:
    using Factory = std::function<std::unique_ptr<View>()>;

    ViewRegistryEntry(std::string name, Factory factory)
        : m_name(std::move(name))
        , m_factory(std::move(factory)) {}

    /// \Returns the name of the view the entry manages
    std::string const& name() const { return m_name; }

    /// Create an empty view object of this view type
    std::unique_ptr<View> create_view(ServiceContext& service_context) const;

private:
    std::string m_name;
    Factory m_factory;
};

/// Static class that manages the view classes in the application
class ViewRegistry {
public:
    /// Adds \p entry to the registry
    static void add(ViewRegistryEntry entry);

    /// \Returns the entry with name \p name if it exists
    static std::optional<ViewRegistryEntry> get(std::string_view name);

    /// \Returns a view over all entries in the registry
    static auto entries() {
        return instance().m_entries.values() | std::views::values;
    }

private:
    ViewRegistry() = default;

    static ViewRegistry& instance();

    utl::hashmap<std::string, ViewRegistryEntry> m_entries;
};

} // namespace mosaic