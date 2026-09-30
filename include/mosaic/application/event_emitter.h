#pragma once

#include <any>
#include <memory>

namespace mosaic {

class EventSystem;

/// Event sender
class EventEmitter {
public:
    EventEmitter() = default;

    explicit EventEmitter(std::shared_ptr<EventSystem> event_system)
        : event_system(std::move(event_system)) {}

    /// Dispatches a message. This function can be called concurrently
    void publish(std::any message) const;

    /// \overload
    void publish_queued(std::any message) const;

private:
    std::shared_ptr<EventSystem> event_system;
};

} // namespace mosaic