#pragma once

#include <any>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
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
    template <typename> struct Tag {};

public:
    EventReceiver() = default;

    explicit EventReceiver(std::weak_ptr<EventSystem> event_system);

    EventReceiver(EventReceiver&&) = default;
    EventReceiver& operator=(EventReceiver&&) = default;

    ~EventReceiver() {
        unregister_all();
    };

    /// Listens to messages of type \p T by invoking \p listener whenever a
    /// message of type \p T is sent. The listener is is unregistered when this
    /// receiver is destroyed or when `unregisterAll()` is called
    ///
    /// \warning This function is not thread safe. Ideally listeners are only
    /// registered from the main thread
    template <typename T, std::invocable<T> F> void listen(F&& listener) {
        auto event_system = event_system_weak.lock();
        if (!event_system) return;
        using U = std::remove_cvref_t<T>;
        do_listen(*event_system, typeid(U),
                  [listener = std::forward<F>(listener)](std::any& message) {
                      std::invoke(listener, std::any_cast<U&>(message));
                  });
    }

    /// \overload
    template <typename F> void listen(F&& listener) {
        using FunctionType = decltype(std::function{listener});
        [&]<typename R, typename T>(Tag<std::function<R(T)>>) {
            listen<T>(std::forward<F>(listener));
        }(Tag<FunctionType>{});
    }

    ///
    void unregister_all();

private:
    void do_listen(EventSystem&, std::type_index,
                   std::function<void(std::any&)>);

    std::weak_ptr<EventSystem> event_system_weak;
    std::vector<ListenerID> registered_ids;
};

} // namespace mosaic

template <> struct std::hash<mosaic::ListenerID> {
    size_t operator()(mosaic::ListenerID ID) const {
        return utl::hash_combine(ID.index, ID.type);
    }
};
