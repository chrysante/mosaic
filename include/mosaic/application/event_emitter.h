#pragma once

#include <memory>
#include <type_traits>
#include <typeindex>

#include <utl/type_erased_object_storage.hpp>

namespace mosaic {

class EventSystem;

/// Event sender
class EventEmitter {
public:
    using Event = utl::type_erased_object_storage;

    EventEmitter() = default;

    explicit EventEmitter(std::shared_ptr<EventSystem> event_system)
        : event_system(std::move(event_system)) {}

    /// Dispatches an event. This function can be called concurrently
    template <typename T> void publish(T&& event) {
        using U = std::remove_cvref_t<T>;
        do_publish(typeid(U), (void*)&event);
    }

    /// \overload
    void publish_queued(Event event) const;

private:
    void do_publish(std::type_index, void*);

    std::shared_ptr<EventSystem> event_system;
};

} // namespace mosaic