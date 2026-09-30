#pragma once

#include <any>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <utl/hashtable.hpp>

#include <mosaic/application/event_emitter.h>
#include <mosaic/application/event_receiver.h>

namespace mosaic {

class EventEmitter;
class EventReceiver;

/// Application event system for communicating facts between otherwise
/// independent components.
///
/// The message system is intended primarily for events: messages that
/// describe something that happened in the application and that unrelated
/// components may be interested in, such as
///
///     ProjectOpened,
///     AssetImported,
///     DocumentSaved,
///     SelectionChanged.
///
/// An event should describe a fact, rather than a particular object that
/// should perform some work. The sender should not need to know which
/// components are listening, and listeners should not need to know which
/// component produced the event.
///
/// ## Architectural rule
///
/// Events describe facts; they do not provide dependencies.
///
/// Do not use the message system as a service locator or dependency injection
/// mechanism. In particular, avoid messages such as
///
///     GetAssetManager,
///     GetRenderer,
///     GetWindow
///
/// or messages containing output pointers/references whose purpose is to
/// retrieve an object from another component.
///
/// If a component requires another component or service in order to perform
/// its work, that is a real dependency and should be expressed explicitly
/// through construction, an application context, or another appropriate
/// dependency mechanism.
///
/// Prefer:
///
///     struct ProjectOpened {
///         std::filesystem::path root;
///     };
///
/// over:
///
///     struct ProjectOpened {
///         AssetManager* asset_manager;
///     };
///
/// The first communicates an application-level fact. The second exposes the
/// implementation that happened to produce that fact.
///
/// ## Events vs. commands
///
/// Events and commands are different concepts:
///
/// Event: "This happened."
/// Command: "Please make this happen."
///
/// An event may have many listeners and the sender should not depend on any
/// particular listener. A command is generally directed at a particular
/// subsystem and should normally be expressed through an explicit interface
/// or dependency rather than broadcast through this system.
///
/// ## Deferred tasks
///
/// Arbitrary callable objects can be useful for scheduling work for later
/// execution, but they are not events. If the system provides a facility for
/// dispatching a std::function<void()>, consider it a task/deferred-work
/// mechanism rather than an application event.
///
/// In particular, do not use arbitrary callable messages as a replacement for
/// ordinary function calls or structured application events.
///
/// ## Synchronous dispatch
///
/// Immediate dispatch executes listeners as part of the caller's call stack.
/// Consequently, dispatching an event may invoke arbitrary application code
/// and may introduce reentrancy. Adding a listener can therefore affect the
/// behavior of an existing synchronous dispatch.
///
/// Use synchronous events when that behavior is intentional; use deferred
/// dispatch when the notification should be processed later.
///
/// ## General guideline
///
/// Use this system when the relationship is naturally expressed as:
///
/// "Something happened, and whoever cares may react."
///
/// Use an explicit dependency when the relationship is:
///
/// "I need this particular object/service to do my job."
///
/// The message system is a decoupling mechanism, not a mechanism for making
/// all dependencies invisible.
class EventSystem : public std::enable_shared_from_this<EventSystem> {
    template <typename> struct Tag {};

public:
    /// Takes private `Tag` class because object must be created through the
    /// static `Create` function
    EventSystem(Tag<void>);

    /// Not copyable
    EventSystem(EventSystem const&) = delete;

    /// Creates an event system. Since emitters and receivers hold shared and
    /// weak references to this class, this shall only exist in shared pointers
    static std::shared_ptr<EventSystem> create();

    /// Immediately calls all registered listeners that listen for the type of
    /// \p message and returns
    void publish(std::any message);

    /// Stores \p message in an internal buffer. This function can be called
    /// concurrently
    void publish_queued(std::any message);

    /// Registers \p listener for listening to any messages sent to this system
    /// Call `unregister` with the returned ID to stop listening
    [[nodiscard]] ListenerID listen(std::type_index type_index,
                                    std::function<void(std::any&)> f);

    /// Calls `publish()` with all messages in the internal buffer
    void flush();

    /// Stops the listener with \p ID from listening to this system
    void unregister(ListenerID ID);

    /// Creates an emitter that emits to this event system
    EventEmitter make_emitter();

    /// Creates an receiver that listens to this event system
    EventReceiver make_receiver();

private:
    using TypedSet = utl::hashmap<int64_t, std::function<void(std::any&)>>;

    utl::hashmap<std::type_index, TypedSet> listener_sets;
    std::mutex buffer_mutex;
    std::vector<std::any> buffer;
    int64_t index = 0;
    bool flushing = false;
};

} // namespace mosaic
