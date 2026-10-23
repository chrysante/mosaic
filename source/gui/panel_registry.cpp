#include "gui/panel_registry.h"

#include "gui/panel.h"
#include "support/logger.h"

using namespace mosaic;

std::unique_ptr<Panel> PanelRegistryEntry::create_panel() const {
    return m_factory();
}

void PanelRegistry::add(std::unique_ptr<PanelRegistryEntry> entry) {
    auto* p = entry.get();
    auto& panel_class = entry->panel_class();
    auto& inst = instance();
    inst.m_entries.push_back(std::move(entry));
    auto [_, inserted] = inst.m_name_map.insert({panel_class.name, p});
    if (!inserted)
        Logger::error("Panel is already registered: ", panel_class.name);
}

PanelRegistryEntry const* PanelRegistry::find(std::string_view name) {
    auto& inst = instance();
    auto itr = inst.m_name_map.find(name);
    if (itr == inst.m_name_map.end()) return nullptr;
    return itr->second;
}

PanelRegistry& PanelRegistry::instance() {
    static PanelRegistry registry;
    return registry;
}