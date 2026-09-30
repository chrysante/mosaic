#pragma once

#include <memory>
#include <typeindex>
#include <vector>

#include <utl/hash.hpp>

namespace mosaic {

class EventSystem;

/// Used to identify registered listeners
struct ListenerID {
    int64_t index;
    std::type_index type;

    bool operator==(ListenerID const&) const = default;
};

/// Event receiver used to listen to events in the application
class EventReceiver {
public:
    EventReceiver() = default;

    explicit EventReceiver(std::weak_ptr<EventSystem> EventSystem);

    EventReceiver(EventReceiver&&) = default;
    EventReceiver& operator=(EventReceiver&&) = default;

    ~EventReceiver() {
        unregister_all();
    };

    /// Listens to messages of type \p T by invoking \p listener whenever a
    /// message of type \p T is sent. The listener is is unregistered when this
    /// receiver is destroyed or when `unregisterAll()` is called
    ///
    /// \Warning This function is not thread safe. Ideally listeners are only
    /// registered from the main thread
    template <typename T, std::invocable<T> F> void listen(F&& listener) {
        listenImpl([&](auto* msg) {
            return msg->template listen<T>(std::forward<F>(listener));
        });
    }

    /// \overload
    template <typename F> void listen(F&& listener) {
        listenImpl(
            [&](auto* msg) { return msg->listen(std::forward<F>(listener)); });
    }

    ///
    void unregister_all();

private:
    void listenImpl(auto callback) {
        if (auto msg = event_system.lock())
            registered_ids.push_back(callback(msg.get()));
    }

    std::weak_ptr<EventSystem> event_system;
    std::vector<ListenerID> registered_ids;
};

} // namespace mosaic

template <> struct std::hash<mosaic::ListenerID> {
    size_t operator()(mosaic::ListenerID ID) const {
        return utl::hash_combine(ID.index, ID.type);
    }
};
