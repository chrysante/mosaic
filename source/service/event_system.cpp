#include "service/event_system.h"

#include "service/event_emitter.h"
#include "service/event_receiver.h"
#include "support/logger.h"

using namespace mosaic;

EventSystem::EventSystem(Tag<void>) {}

std::shared_ptr<EventSystem> EventSystem::create() {
    return std::make_shared<EventSystem>(Tag<void>{});
}

void EventSystem::do_publish(std::type_index type_index, void* event) {
    auto& set = listener_sets[type_index];
    for (auto& [index, listener] : set)
        listener(event);
}

void EventSystem::publish_queued(Event event) {
    std::lock_guard lock(buffer_mutex);
    buffer.push_back(std::move(event));
}

void EventSystem::flush() {
    /// Guard this here to prevent feedback loops
    if (flushing) return;
    flushing = true;
    auto events = [this] {
        std::lock_guard lock(buffer_mutex);
        return std::move(buffer);
    }();
    for (auto& event : events) {
        try {
            do_publish(event.type(), event.data());
        } catch (std::exception const& e) {
            Logger::error("Dispatched message failed with expection: ",
                          e.what());
        }
    }
    flushing = false;
}

ListenerID EventSystem::do_listen(std::type_index type_index,
                                  std::function<void(void*)> f) {
    auto local_index = index++;
    listener_sets[type_index][local_index] = std::move(f);
    return ListenerID{local_index, type_index};
}

void EventSystem::unregister(ListenerID id) {
    auto itr = listener_sets.find(id.type);
    assert(itr != listener_sets.end() && "Invalid ID");
    auto& set = itr->second;
    auto setItr = set.find(id.index);
    assert(setItr != set.end() && "Invalid ID");
    set.erase(setItr);
}

EventEmitter EventSystem::make_emitter() {
    return EventEmitter(shared_from_this());
}

EventReceiver EventSystem::make_receiver() {
    return EventReceiver(shared_from_this());
}
