#pragma once

#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

#include <utl/common.hpp>
#include <utl/hashtable.hpp>

namespace mosaic {

class ServiceContext;
class Panel;
struct PanelClass;

class PanelRegistryEntry {
public:
    using Factory = std::function<std::unique_ptr<Panel>()>;

    PanelRegistryEntry(PanelClass const& panel_class, Factory factory)
        : m_panel_class(&panel_class)
        , m_factory(std::move(factory)) {}

    PanelClass const& panel_class() const { return *m_panel_class; }

    std::unique_ptr<Panel> create_panel() const;

private:
    PanelClass const* m_panel_class;
    Factory m_factory;

    friend class PanelRegistry;
};

class PanelRegistry {
public:
    static void add(std::unique_ptr<PanelRegistryEntry> entry);

    static PanelRegistryEntry const* find(std::string_view name);

    static auto entries() {
        return instance().m_name_map.values() | std::views::values;
    }

private:
    PanelRegistry() = default;

    static PanelRegistry& instance();

    std::vector<std::unique_ptr<PanelRegistryEntry>> m_entries;
    utl::hashmap<std::string, PanelRegistryEntry const*> m_name_map;
};

#define MOSAIC_REGISTER_VIEW(Type, panel_class_init)                           \
    UTL_STATIC_INIT {                                                          \
        static ::mosaic::PanelClass panel_class = panel_class_init;            \
        ::mosaic::PanelRegistry::add(                                          \
            std::make_unique<::mosaic::PanelRegistryEntry>(                    \
                panel_class, [] { return std::make_unique<Type>(); }));        \
    }

} // namespace mosaic
