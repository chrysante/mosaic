#include "application/event_emitter.h"

#include <cassert>

#include "application/event_system.h"

using namespace mosaic;

void EventEmitter::publish(std::any event) const {
    assert(event_system && "Cannot dispatch without an event system");
    event_system->publish(std::move(event));
}

void EventEmitter::publish_queued(std::any event) const {
    assert(event_system && "Cannot dispatch without an event system");
    event_system->publish_queued(std::move(event));
}