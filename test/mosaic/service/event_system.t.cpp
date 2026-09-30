#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <mosaic/service/event_emitter.h>
#include <mosaic/service/event_receiver.h>
#include <mosaic/service/event_system.h>

using namespace mosaic;

namespace {

struct ProjectOpened { std::string root; };

struct AssetImported { int asset_id; };

struct SelectionChanged { int selection_id; };

struct UnrelatedEvent { int value; };

} // namespace

TEST_CASE("EventSystem can be created", "[event_system]") {
    auto event_system = EventSystem::create();
    REQUIRE(event_system);
}

TEST_CASE("Publishing an event invokes a matching listener", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    bool received = false;
    std::string received_root;
    receiver.listen<ProjectOpened>([&](ProjectOpened const& event) {
        received = true;
        received_root = event.root;
    });
    emitter.publish(ProjectOpened{"test/project"});
    REQUIRE(received);
    REQUIRE(received_root == "test/project");
}

TEST_CASE("Publishing an event is synchronous", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    bool received = false;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { received = true; });
    REQUIRE_FALSE(received);
    emitter.publish(ProjectOpened{"project"});
    // publish() must have called the listener before returning.
    REQUIRE(received);
}

TEST_CASE("Only listeners for the event type receive an event",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    int project_opened_count = 0;
    int asset_imported_count = 0;
    int unrelated_count = 0;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++project_opened_count; });
    receiver.listen<AssetImported>(
        [&](AssetImported const&) { ++asset_imported_count; });
    receiver.listen<UnrelatedEvent>(
        [&](UnrelatedEvent const&) { ++unrelated_count; });
    emitter.publish(ProjectOpened{"project"});
    REQUIRE(project_opened_count == 1);
    REQUIRE(asset_imported_count == 0);
    REQUIRE(unrelated_count == 0);
}

TEST_CASE("Multiple listeners receive the same event", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    int first_count = 0;
    int second_count = 0;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++first_count; });
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++second_count; });
    emitter.publish(ProjectOpened{"project"});
    REQUIRE(first_count == 1);
    REQUIRE(second_count == 1);
}

TEST_CASE("A listener receives every published event of its type",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::vector<int> received_ids;
    receiver.listen<AssetImported>([&](AssetImported const& event) {
        received_ids.push_back(event.asset_id);
    });
    emitter.publish(AssetImported{1});
    emitter.publish(AssetImported{2});
    emitter.publish(AssetImported{3});
    REQUIRE(received_ids == std::vector<int>{1, 2, 3});
}

TEST_CASE("A listener can be registered using an inferred callable signature",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    int received_id = 0;
    receiver.listen(
        [&](AssetImported const& event) { received_id = event.asset_id; });
    emitter.publish(AssetImported{42});
    REQUIRE(received_id == 42);
}

TEST_CASE("Different emitters can publish to the same event system",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto first_emitter = event_system->make_emitter();
    auto second_emitter = event_system->make_emitter();
    int count = 0;
    receiver.listen<ProjectOpened>([&](ProjectOpened const&) { ++count; });
    first_emitter.publish(ProjectOpened{"first"});
    second_emitter.publish(ProjectOpened{"second"});
    REQUIRE(count == 2);
}

TEST_CASE("Queued events are not delivered until flush", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    int received_id = 0;
    receiver.listen<AssetImported>(
        [&](AssetImported const& event) { received_id = event.asset_id; });
    emitter.publish_queued(EventSystem::Event{AssetImported{42}});
    REQUIRE(received_id == 0);
    event_system->flush();
    REQUIRE(received_id == 42);
}

TEST_CASE("Multiple queued events are delivered in order", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::vector<int> received_ids;
    receiver.listen<AssetImported>([&](AssetImported const& event) {
        received_ids.push_back(event.asset_id);
    });
    emitter.publish_queued(EventSystem::Event{AssetImported{1}});
    emitter.publish_queued(EventSystem::Event{AssetImported{2}});
    emitter.publish_queued(EventSystem::Event{AssetImported{3}});
    REQUIRE(received_ids.empty());
    event_system->flush();
    REQUIRE(received_ids == std::vector<int>{1, 2, 3});
}

TEST_CASE("Queued events of different types are dispatched correctly",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::vector<int> imported_ids;
    std::vector<std::string> opened_projects;
    receiver.listen<AssetImported>([&](AssetImported const& event) {
        imported_ids.push_back(event.asset_id);
    });
    receiver.listen<ProjectOpened>([&](ProjectOpened const& event) {
        opened_projects.push_back(event.root);
    });
    emitter.publish_queued(EventSystem::Event{AssetImported{1}});
    emitter.publish_queued(EventSystem::Event{ProjectOpened{"one"}});
    emitter.publish_queued(EventSystem::Event{AssetImported{2}});
    emitter.publish_queued(EventSystem::Event{ProjectOpened{"two"}});
    event_system->flush();
    REQUIRE(imported_ids == std::vector<int>{1, 2});
    REQUIRE(opened_projects == std::vector<std::string>{"one", "two"});
}

TEST_CASE("Calling flush with an empty queue does nothing", "[event_system]") {
    auto event_system = EventSystem::create();
    REQUIRE_NOTHROW(event_system->flush());
}

TEST_CASE("Destroying a receiver unregisters its listeners", "[event_system]") {
    auto event_system = EventSystem::create();
    auto emitter = event_system->make_emitter();
    int count = 0;
    {
        auto receiver = event_system->make_receiver();
        receiver.listen<ProjectOpened>([&](ProjectOpened const&) { ++count; });
        emitter.publish(ProjectOpened{"before"});
        REQUIRE(count == 1);
    }
    emitter.publish(ProjectOpened{"after"});
    REQUIRE(count == 1);
}

TEST_CASE("unregister_all removes all listeners from a receiver",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    int project_count = 0;
    int asset_count = 0;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++project_count; });
    receiver.listen<AssetImported>(
        [&](AssetImported const&) { ++asset_count; });
    emitter.publish(ProjectOpened{"before"});
    emitter.publish(AssetImported{1});
    REQUIRE(project_count == 1);
    REQUIRE(asset_count == 1);
    receiver.unregister_all();
    emitter.publish(ProjectOpened{"after"});
    emitter.publish(AssetImported{2});
    REQUIRE(project_count == 1);
    REQUIRE(asset_count == 1);
}

TEST_CASE("Destroying one receiver leaves the other receiver active",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto emitter = event_system->make_emitter();
    int first_count = 0;
    int second_count = 0;
    auto first_receiver = event_system->make_receiver();
    auto second_receiver = event_system->make_receiver();
    first_receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++first_count; });
    second_receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { ++second_count; });
    emitter.publish(ProjectOpened{"before"});
    REQUIRE(first_count == 1);
    REQUIRE(second_count == 1);
    first_receiver.unregister_all();
    emitter.publish(ProjectOpened{"after"});
    REQUIRE(first_count == 1);
    REQUIRE(second_count == 2);
}

TEST_CASE("unregister_all is safe to call repeatedly", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    receiver.unregister_all();
    receiver.unregister_all();
    REQUIRE_NOTHROW(receiver.unregister_all());
}

TEST_CASE("An emitter keeps the event system alive", "[event_system]") {
    auto event_system = EventSystem::create();
    auto emitter = event_system->make_emitter();
    int count = 0;
    {
        auto receiver = event_system->make_receiver();
        receiver.listen<ProjectOpened>([&](ProjectOpened const&) { ++count; });
        // The emitter owns a shared_ptr to the event system, so destroying
        // the original shared_ptr should not destroy the event system.
        event_system.reset();
        emitter.publish(ProjectOpened{"still-alive"});
    }
    REQUIRE(count == 1);
}

TEST_CASE("A receiver does not prevent the event system from being destroyed",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    int count = 0;
    receiver.listen<ProjectOpened>([&](ProjectOpened const&) { ++count; });
    // receiver only has a weak_ptr.
    event_system.reset();
    // The receiver should now simply stop doing anything when unregistering.
    REQUIRE_NOTHROW(receiver.unregister_all());
}

TEST_CASE("Publishing through EventSystem directly reaches receivers",
          "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    int received_id = 0;
    receiver.listen<AssetImported>(
        [&](AssetImported const& event) { received_id = event.asset_id; });
    event_system->publish(AssetImported{123});
    REQUIRE(received_id == 123);
}

TEST_CASE("Event values are delivered correctly", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::string received_root;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const& event) { received_root = event.root; });
    emitter.publish(ProjectOpened{
        .root = "/some/project/path",
    });
    REQUIRE(received_root == "/some/project/path");
}

TEST_CASE("Listeners are invoked in registration order", "[event_system]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::vector<int> invocation_order;
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { invocation_order.push_back(1); });
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { invocation_order.push_back(2); });
    receiver.listen<ProjectOpened>(
        [&](ProjectOpened const&) { invocation_order.push_back(3); });
    emitter.publish(ProjectOpened{"project"});
    REQUIRE(invocation_order == std::vector<int>{1, 2, 3});
}

TEST_CASE("Publishing can be done concurrently", "[event_system][threading]") {
    auto event_system = EventSystem::create();
    auto receiver = event_system->make_receiver();
    auto emitter = event_system->make_emitter();
    std::atomic<int> received_count{0};
    receiver.listen<AssetImported>(
        [&](AssetImported const&) { ++received_count; });
    constexpr int thread_count = 8;
    constexpr int events_per_thread = 1000;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([&] {
            for (int i = 0; i < events_per_thread; ++i) {
                emitter.publish(AssetImported{i});
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    REQUIRE(received_count == thread_count * events_per_thread);
}
