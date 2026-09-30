#include "service/event_receiver.h"

#include "service/event_system.h"

using namespace mosaic;

EventReceiver::EventReceiver(std::weak_ptr<EventSystem> event_system)
    : event_system_weak(std::move(event_system)) {}

void EventReceiver::do_listen(EventSystem& event_system,
                              std::type_index type_index,
                              std::function<void(void*)> f) {
    registered_ids.push_back(event_system.do_listen(type_index, std::move(f)));
}

void EventReceiver::unregister_all() {
    auto event_system = event_system_weak.lock();
    if (!event_system) return;
    for (auto& id : registered_ids)
        event_system->unregister(id);
    registered_ids.clear();
}
