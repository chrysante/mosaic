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
class View;
struct ViewClass;

class ViewRegistryEntry {
public:
    using Factory = std::function<std::unique_ptr<View>()>;

    ViewRegistryEntry(ViewClass const& view_class, Factory factory)
        : m_view_class(&view_class)
        , m_factory(std::move(factory)) {}

    ViewClass const& view_class() const { return *m_view_class; }

    std::unique_ptr<View> create_view() const;

private:
    ViewClass const* m_view_class;
    Factory m_factory;

    friend class ViewRegistry;
};

class ViewRegistry {
public:
    static void add(std::unique_ptr<ViewRegistryEntry> entry);

    static ViewRegistryEntry const* find(std::string_view name);

    static auto entries() {
        return instance().m_name_map.values() | std::views::values;
    }

private:
    ViewRegistry() = default;

    static ViewRegistry& instance();

    std::vector<std::unique_ptr<ViewRegistryEntry>> m_entries;
    utl::hashmap<std::string, ViewRegistryEntry const*> m_name_map;
};

#define MOSAIC_REGISTER_VIEW(Type, view_class_init)                            \
    UTL_STATIC_INIT {                                                          \
        static ::mosaic::ViewClass view_class = view_class_init;               \
        ::mosaic::ViewRegistry::add(                                           \
            std::make_unique<::mosaic::ViewRegistryEntry>(                     \
                view_class, [] { return std::make_unique<Type>(); }));         \
    }

} // namespace mosaic
