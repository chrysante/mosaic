#include "gui/view_registry.h"

#include "application/application.h"
#include "gui/view.h"
#include "service/service_context.h"
#include "support/logger.h"

using namespace mosaic;

std::unique_ptr<View>
ViewRegistryEntry::create_view(ServiceContext& service_context) const {
    auto view = m_factory();
    view->m_data.title = name();
    view->m_data.static_name = name();
    view->m_data.service_context = &service_context;
    view->m_event_emitter = service_context.make_event_emitter();
    view->m_event_receiver = service_context.make_event_receiver();
    return view;
}

void ViewRegistry::add(ViewRegistryEntry entry) {
    bool success = instance().m_entries.insert({entry.name(), entry}).second;
    if (!success) Logger::error("View is already registered: ", entry.name());
}

std::optional<ViewRegistryEntry> ViewRegistry::get(std::string_view name) {
    auto& inst = instance();
    auto itr = inst.m_entries.find(name);
    if (itr == inst.m_entries.end()) return std::nullopt;
    return itr->second;
}

ViewRegistry& ViewRegistry::instance() {
    static ViewRegistry instance;
    return instance;
}
