#include "gui/view_registry.h"

#include "gui/view.h"
#include "service/service_context.h"
#include "support/logger.h"

using namespace mosaic;

std::unique_ptr<View>
ViewRegistryEntry::create_view(ServiceContext& services) const {
    /*
     * The registry only constructs the object.
     *
     * ViewManager initializes its identity and lifecycle.
     */
    (void)services;

    return m_factory();
}

void ViewRegistry::add(ViewRegistryEntry entry) {
    auto [_, inserted] =
        instance().m_entries.insert({entry.name(), std::move(entry)});

    if (!inserted) {
        Logger::error("View is already registered: ", entry.name());
    }
}

std::optional<ViewRegistryEntry> ViewRegistry::get(std::string_view name) {
    auto& registry = instance();

    auto itr = registry.m_entries.find(name);

    if (itr == registry.m_entries.end()) return std::nullopt;

    return itr->second;
}

ViewRegistry& ViewRegistry::instance() {
    static ViewRegistry registry;
    return registry;
}