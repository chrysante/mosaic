#include "application/event_receiver.h"

#include "application/event_system.h"

using namespace mosaic;

EventReceiver::EventReceiver(std::weak_ptr<EventSystem> event_system)
    : event_system(std::move(event_system)) {}

void EventReceiver::unregister_all() {
    auto msg = event_system.lock();
    if (!msg)
        return;
    for (auto& id : registered_ids)
        msg->unregister(id);
    registered_ids.clear();
}
