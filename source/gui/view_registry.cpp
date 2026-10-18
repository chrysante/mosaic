#include "gui/view_registry.h"

#include "gui/view.h"
#include "support/logger.h"

using namespace mosaic;

std::unique_ptr<View> ViewRegistryEntry::create_view() const {
    return m_factory();
}

void ViewRegistry::add(std::unique_ptr<ViewRegistryEntry> entry) {
    auto* p = entry.get();
    auto& view_class = entry->view_class();
    auto& inst = instance();
    inst.m_entries.push_back(std::move(entry));
    auto [_, inserted] = inst.m_name_map.insert({view_class.name, p});
    if (!inserted)
        Logger::error("View is already registered: ", view_class.name);
}

ViewRegistryEntry const* ViewRegistry::find(std::string_view name) {
    auto& inst = instance();
    auto itr = inst.m_name_map.find(name);
    if (itr == inst.m_name_map.end()) return nullptr;
    return itr->second;
}

ViewRegistry& ViewRegistry::instance() {
    static ViewRegistry registry;
    return registry;
}