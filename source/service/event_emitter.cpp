#include "service/event_emitter.h"

#include <cassert>

#include "service/event_system.h"

using namespace mosaic;

void EventEmitter::publish_queued(Event event) const {
    assert(event_system && "Cannot publish without an event system");
    event_system->publish_queued(std::move(event));
}

void EventEmitter::do_publish(std::type_index type_index, void* event) {
    assert(event_system && "Cannot publish without an event system");
    event_system->do_publish(type_index, event);
}