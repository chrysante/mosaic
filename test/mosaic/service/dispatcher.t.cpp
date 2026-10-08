#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstddef>
#include <latch>
#include <vector>

#include <mosaic/service/dispatcher.h>

using namespace mosaic;

namespace {

using namespace std::chrono_literals;

struct DispatcherFixture {
    explicit DispatcherFixture(std::size_t workers = 4)
        : pool(workers)
        , dispatcher(pool) {}

    ~DispatcherFixture() { dispatcher.shutdown(); }

    ThreadPool pool;
    WorkDispatcher dispatcher;
};

} // namespace

// ============================================================================
// Completion
// ============================================================================

TEST_CASE("Completion default construction is ready",
          "[dispatcher][completion]") {
    Completion completion;
    REQUIRE_FALSE(completion);
    REQUIRE(completion.ready());
    // A default completion represents no work, so waiting should be harmless.
    completion.wait();
}

TEST_CASE("Completion is not ready while work is running",
          "[dispatcher][completion][dispatch]") {
    DispatcherFixture fixture;
    std::latch started(1);
    std::latch release(1);
    Completion completion = fixture.dispatcher.dispatch_waitable([&] {
        started.count_down();
        release.wait();
    });
    started.wait();
    REQUIRE(completion);
    REQUIRE_FALSE(completion.ready());
    release.count_down();
    completion.wait();
    REQUIRE(completion.ready());
}
TEST_CASE("Completion can be waited on multiple times",
          "[dispatcher][completion]") {
    DispatcherFixture fixture;
    Completion completion = fixture.dispatcher.dispatch_waitable([] {});
    completion.wait();
    REQUIRE(completion.ready());
    // Waiting again must remain safe.
    completion.wait();
    REQUIRE(completion.ready());
}

TEST_CASE("Completion is copyable and shares completion state",
          "[dispatcher][completion]") {
    DispatcherFixture fixture;
    std::latch started(1);
    std::latch release(1);
    Completion first = fixture.dispatcher.dispatch_waitable([&] {
        started.count_down();
        release.wait();
    });
    Completion second = first;
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE_FALSE(first.ready());
    REQUIRE_FALSE(second.ready());
    started.wait();
    // Neither handle should report completion while the task is blocked.
    REQUIRE_FALSE(first.ready());
    REQUIRE_FALSE(second.ready());
    release.count_down();
    first.wait();
    REQUIRE(first.ready());
    REQUIRE(second.ready());
    second.wait();
}

TEST_CASE("Completion move transfers ownership", "[dispatcher][completion]") {
    DispatcherFixture fixture;
    Completion original = fixture.dispatcher.dispatch_waitable([] {});
    REQUIRE(original);
    Completion moved = std::move(original);
    REQUIRE_FALSE(original);
    REQUIRE(moved);
    moved.wait();
    REQUIRE(moved.ready());
}

// ============================================================================
// Fire-and-forget dispatch
// ============================================================================

TEST_CASE("dispatch executes submitted work", "[dispatcher][dispatch]") {
    DispatcherFixture fixture;
    std::atomic<int> value{0};
    fixture.dispatcher.dispatch(
        [&] { value.fetch_add(1, std::memory_order_relaxed); });
    fixture.dispatcher.shutdown();
    REQUIRE(value.load(std::memory_order_relaxed) == 1);
}

// ============================================================================
// Main-thread work
// ============================================================================

TEST_CASE("on_main_thread does not execute immediately",
          "[dispatcher][main-thread]") {
    DispatcherFixture fixture;
    bool executed = false;
    fixture.dispatcher.on_main_thread([&] { executed = true; });
    REQUIRE_FALSE(executed);
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(executed);
}

TEST_CASE("process_main_thread_tasks executes queued work",
          "[dispatcher][main-thread]") {
    DispatcherFixture fixture;
    std::vector<int> values;
    fixture.dispatcher.on_main_thread([&] { values.push_back(1); });
    fixture.dispatcher.on_main_thread([&] { values.push_back(2); });
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(values == std::vector<int>{1, 2});
}

TEST_CASE("main-thread work queued during processing is deferred",
          "[dispatcher][main-thread]") {
    DispatcherFixture fixture;
    std::vector<int> values;
    fixture.dispatcher.on_main_thread([&] {
        values.push_back(1);
        fixture.dispatcher.on_main_thread([&] { values.push_back(3); });
    });
    fixture.dispatcher.on_main_thread([&] { values.push_back(2); });
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(values == std::vector<int>{1, 2});
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(values == std::vector<int>{1, 2, 3});
}

TEST_CASE("main-thread waitable completion becomes ready after execution",
          "[dispatcher][main-thread][completion]") {
    DispatcherFixture fixture;
    bool executed = false;
    Completion completion =
        fixture.dispatcher.on_main_thread_waitable([&] { executed = true; });
    REQUIRE(completion);
    REQUIRE_FALSE(completion.ready());
    REQUIRE_FALSE(executed);
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(executed);
    REQUIRE(completion.ready());
    completion.wait();
}

TEST_CASE("multiple main-thread waitable completions remain independent",
          "[dispatcher][main-thread][completion]") {
    DispatcherFixture fixture;
    int count = 0;
    Completion first =
        fixture.dispatcher.on_main_thread_waitable([&] { count += 1; });
    Completion second =
        fixture.dispatcher.on_main_thread_waitable([&] { count += 1; });
    REQUIRE_FALSE(first.ready());
    REQUIRE_FALSE(second.ready());
    fixture.dispatcher.process_main_thread_tasks();
    REQUIRE(first.ready());
    REQUIRE(second.ready());
    REQUIRE(count == 2);
}

// ============================================================================
// parallel_for
// ============================================================================

TEST_CASE("parallel_for executes every index exactly once",
          "[dispatcher][parallel]") {
    DispatcherFixture fixture;
    constexpr std::size_t count = 1000;
    std::vector<std::atomic<int>> visits(count);
    for (auto& value : visits)
        value.store(0, std::memory_order_relaxed);
    Completion completion =
        fixture.dispatcher.parallel_for_waitable(count, [&](std::size_t index) {
            visits[index].fetch_add(1, std::memory_order_relaxed);
        });
    completion.wait();
    for (std::size_t i = 0; i < count; ++i)
        REQUIRE(visits[i].load(std::memory_order_relaxed) == 1);
}

TEST_CASE("parallel_for_waitable becomes ready after all work completes",
          "[dispatcher][parallel][completion]") {
    DispatcherFixture fixture;
    constexpr std::size_t count = 100;
    std::atomic<std::size_t> completed{0};
    Completion completion =
        fixture.dispatcher.parallel_for_waitable(count, [&](std::size_t) {
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    REQUIRE(completion);
    completion.wait();
    REQUIRE(completed.load(std::memory_order_relaxed) == count);
    REQUIRE(completion.ready());
}

TEST_CASE("parallel_for with explicit chunk size covers all elements",
          "[dispatcher][parallel][chunking]") {
    DispatcherFixture fixture;
    constexpr std::size_t count = 103;
    constexpr std::size_t chunk_size = 10;
    std::vector<std::atomic<int>> visits(count);
    for (auto& value : visits)
        value.store(0, std::memory_order_relaxed);
    Completion completion = fixture.dispatcher.parallel_for_waitable(
        count,
        [&](std::size_t index) {
            visits[index].fetch_add(1, std::memory_order_relaxed);
        },
        ParallelOptions{
            .chunk_size = chunk_size,
        });
    completion.wait();
    for (std::size_t i = 0; i < count; ++i)
        REQUIRE(visits[i].load(std::memory_order_relaxed) == 1);
}

TEST_CASE("parallel_for handles one element", "[dispatcher][parallel][edge]") {
    DispatcherFixture fixture;
    std::atomic<int> calls{0};
    Completion completion =
        fixture.dispatcher.parallel_for_waitable(1, [&](std::size_t index) {
            REQUIRE(index == 0);
            calls.fetch_add(1, std::memory_order_relaxed);
        });
    completion.wait();
    REQUIRE(calls.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("parallel_for handles count smaller than requested chunks",
          "[dispatcher][parallel][edge]") {
    DispatcherFixture fixture(8);
    constexpr std::size_t count = 3;
    std::vector<std::atomic<int>> visits(count);
    for (auto& value : visits)
        value.store(0, std::memory_order_relaxed);
    Completion completion =
        fixture.dispatcher.parallel_for_waitable(count, [&](std::size_t index) {
            visits[index].fetch_add(1, std::memory_order_relaxed);
        });
    completion.wait();
    for (std::size_t i = 0; i < count; ++i)
        REQUIRE(visits[i].load(std::memory_order_relaxed) == 1);
}

TEST_CASE("parallel_for with zero count does nothing",
          "[dispatcher][parallel][edge]") {
    DispatcherFixture fixture;
    std::atomic<bool> called{false};
    Completion completion = fixture.dispatcher.parallel_for_waitable(
        0, [&](std::size_t) { called.store(true, std::memory_order_release); });
    REQUIRE_FALSE(completion);
    REQUIRE(completion.ready());
    REQUIRE_FALSE(called.load(std::memory_order_acquire));
}

// ============================================================================
// Range overloads
// ============================================================================

TEST_CASE("parallel_for range overload processes every element",
          "[dispatcher][parallel][range]") {
    DispatcherFixture fixture;
    std::vector<int> values(100, 0);
    Completion completion = fixture.dispatcher.parallel_for_waitable(
        values, [](int& value) { value = 42; });
    completion.wait();
    for (int value : values)
        REQUIRE(value == 42);
}

TEST_CASE("parallel_for range overload can modify elements independently",
          "[dispatcher][parallel][range]") {
    DispatcherFixture fixture;
    std::vector<int> values(256);
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = static_cast<int>(i);
    Completion completion = fixture.dispatcher.parallel_for_waitable(
        values, [](int& value) { value *= 2; });
    completion.wait();
    for (std::size_t i = 0; i < values.size(); ++i)
        REQUIRE(values[i] == static_cast<int>(i * 2));
}

// ============================================================================
// Concurrency
// ============================================================================

TEST_CASE("parallel_for actually permits concurrent execution",
          "[dispatcher][parallel][concurrency]") {
    DispatcherFixture fixture(4);
    constexpr std::size_t count = 4;
    std::atomic<int> running{0};
    std::atomic<int> maximum_running{0};
    Completion completion = fixture.dispatcher.parallel_for_waitable(
        count,
        [&](std::size_t) {
            int const current =
                running.fetch_add(1, std::memory_order_acq_rel) + 1;
            int observed = maximum_running.load(std::memory_order_relaxed);
            while (current > observed &&
                   !maximum_running.compare_exchange_weak(
                       observed, current, std::memory_order_relaxed,
                       std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(10ms);
            running.fetch_sub(1, std::memory_order_acq_rel);
        },
        ParallelOptions{
            .chunk_size = 1,
        });
    completion.wait();
    REQUIRE(maximum_running.load(std::memory_order_relaxed) >= 2);
}
// ============================================================================
// Completion lifetime / reference counting
// ============================================================================

TEST_CASE("destroying Completion before work finishes is safe",
          "[dispatcher][completion][lifetime]") {
    DispatcherFixture fixture;
    std::latch started(1);
    std::latch release(1);
    std::atomic<bool> finished{false};
    {
        Completion completion = fixture.dispatcher.dispatch_waitable([&] {
            started.count_down();
            release.wait();
            finished.store(true, std::memory_order_release);
        });
        started.wait();
        // The Completion handle goes away while the worker still owns the
        // state through its internal CompletionStateHandle.
    }
    release.count_down();
    // The worker must still be able to complete and release its state.
    while (!finished.load(std::memory_order_acquire))
        std::this_thread::yield();
    REQUIRE(finished.load(std::memory_order_acquire));
}

TEST_CASE("parallel completion state survives destruction of Completion handle",
          "[dispatcher][completion][lifetime][parallel]") {
    DispatcherFixture fixture(4);
    std::latch started(2);
    std::latch release(1);
    std::atomic<std::size_t> completed{0};
    {
        Completion completion = fixture.dispatcher.parallel_for_waitable(
            2,
            [&](std::size_t) {
                started.count_down();
                release.wait();
                completed.fetch_add(1, std::memory_order_relaxed);
            },
            ParallelOptions{
                .chunk_size = 1,
            });
        started.wait();
        // All worker tasks should now have acquired their own references.
        // The public Completion is deliberately destroyed here.
    }
    release.count_down();
    while (completed.load(std::memory_order_relaxed) != 2)
        std::this_thread::yield();
    REQUIRE(completed.load(std::memory_order_relaxed) == 2);
}

// ============================================================================
// Reuse of completion-state pool
// ============================================================================

TEST_CASE("completion states can be repeatedly acquired and released",
          "[dispatcher][completion][pool]") {
    // Small page size forces the pool to allocate/reuse pages frequently.
    DispatcherFixture fixture;
    for (int iteration = 0; iteration < 10'000; ++iteration) {
        Completion completion = fixture.dispatcher.dispatch_waitable([] {});
        completion.wait();
        REQUIRE(completion.ready());
    }
}

TEST_CASE(
    "parallel completions can force multiple completion-state allocations",
    "[dispatcher][completion][pool][parallel]") {
    DispatcherFixture fixture;
    constexpr std::size_t iterations = 100;
    constexpr std::size_t elements = 64;
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        std::atomic<std::size_t> count{0};
        Completion completion = fixture.dispatcher.parallel_for_waitable(
            elements,
            [&](std::size_t) { count.fetch_add(1, std::memory_order_relaxed); },
            ParallelOptions{
                .chunk_size = 1,
            });
        completion.wait();
        REQUIRE(count.load(std::memory_order_relaxed) == elements);
    }
}
