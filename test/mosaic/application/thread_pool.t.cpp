#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <latch>
#include <mutex>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <mosaic/application/thread_pool.h>

using namespace mosaic;
using namespace std::chrono_literals;

TEST_CASE("ThreadPool can be constructed", "[thread_pool]") {
    SECTION("default constructor") {
        ThreadPool pool;
        pool.submit([] {});
        pool.shutdown();
    }
    SECTION("explicit thread count") {
        ThreadPool pool(4);
        pool.submit([] {});
        pool.shutdown();
    }
    SECTION("zero thread count") {
        ThreadPool pool(0);
        std::atomic<bool> executed{false};
        pool.submit([&] { executed.store(true, std::memory_order_release); });
        pool.shutdown();
        REQUIRE(executed.load(std::memory_order_acquire));
    }
}

TEST_CASE("ThreadPool executes tasks", "[thread_pool]") {
    ThreadPool pool(4);
    SECTION("single task") {
        std::atomic<bool> executed{false};
        pool.submit([&] { executed.store(true, std::memory_order_release); });
        pool.shutdown();
        REQUIRE(executed.load(std::memory_order_acquire));
    }
    SECTION("many tasks") {
        constexpr std::size_t task_count = 10'000;
        std::atomic<std::size_t> completed{0};
        for (std::size_t i = 0; i < task_count; ++i)
            pool.submit(
                [&] { completed.fetch_add(1, std::memory_order_relaxed); });
        pool.shutdown();
        REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
    }
    SECTION("tasks execute exactly once") {
        constexpr std::size_t task_count = 10'000;
        std::vector<std::atomic<int>> executions(task_count);
        for (auto& execution : executions)
            execution.store(0);
        for (std::size_t i = 0; i < task_count; ++i)
            pool.submit([&, i] {
                executions[i].fetch_add(1, std::memory_order_relaxed);
            });
        pool.shutdown();
        for (std::size_t i = 0; i < task_count; ++i)
            REQUIRE(executions[i].load(std::memory_order_relaxed) == 1);
    }

    SECTION("tasks with different durations") {
        constexpr std::size_t task_count = 1'000;
        std::atomic<std::size_t> completed{0};
        for (std::size_t i = 0; i < task_count; ++i) {
            pool.submit([&, i] {
                if (i % 10 == 0)
                    std::this_thread::sleep_for(1ms);
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }
        pool.shutdown();
        REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
    }
}

TEST_CASE("ThreadPool executes tasks concurrently", "[thread_pool]") {
    // Main
    //  |
    //  +-- submit 4 tasks
    //        |
    //        +--> Worker 0 ──┐
    //        +--> Worker 1 ──┤
    //        +--> Worker 2 ──┼──> workers_started
    //        +--> Worker 3 ──┘
    //                          |
    //                          v
    //                    main continues
    //                          |
    //                          v
    //                  release_workers
    //                          |
    //               ┌──────────┼──────────┐
    //               v          v          v
    //            Worker 0   Worker 1   Worker 2 ...
    //               |          |          |
    //               +----------+----------+
    //                          |
    //                     running > 1
    constexpr int worker_count = 4;
    ThreadPool pool(worker_count);
    std::atomic<int> running{0};
    std::atomic<int> maximum_running{0};
    std::latch workers_started(worker_count);
    std::latch release_workers(1);
    for (int i = 0; i < worker_count; ++i) {
        pool.submit([&] {
            workers_started.count_down();
            release_workers.wait();
            int const current =
                running.fetch_add(1, std::memory_order_relaxed) + 1;
            int previous = maximum_running.load(std::memory_order_relaxed);
            while (current > previous &&
                   !maximum_running.compare_exchange_weak(
                       previous, current, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(10ms);
            running.fetch_sub(1, std::memory_order_relaxed);
        });
    }
    workers_started.wait();
    release_workers.count_down();
    pool.shutdown();
    REQUIRE(maximum_running.load(std::memory_order_relaxed) > 1);
}

TEST_CASE("ThreadPool supports recursive task submission", "[thread_pool]") {
    ThreadPool pool(4);
    constexpr int depth = 1'000;
    std::atomic<int> completed{0};
    std::function<void(int)> submit_recursive;
    submit_recursive = [&](int remaining) {
        pool.submit([&, remaining] {
            completed.fetch_add(1, std::memory_order_relaxed);
            if (remaining > 0)
                submit_recursive(remaining - 1);
        });
    };
    submit_recursive(depth);
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == depth + 1);
}

TEST_CASE("ThreadPool waits for recursively submitted tasks", "[thread_pool]") {
    ThreadPool pool(4);
    constexpr int depth = 1'000;
    std::atomic<int> completed{0};
    std::function<void(int)> submit_recursive;
    submit_recursive = [&](int remaining) {
        pool.submit([&, remaining] {
            completed.fetch_add(1, std::memory_order_relaxed);
            if (remaining > 0)
                submit_recursive(remaining - 1);
        });
    };
    submit_recursive(depth);
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == depth + 1);
}

TEST_CASE("ThreadPool handles recursive task fan-out", "[thread_pool]") {
    ThreadPool pool(8);
    constexpr int depth = 8;
    constexpr int fanout = 4;
    std::atomic<std::size_t> completed{0};
    std::function<void(int)> submit_tree;
    submit_tree = [&](int remaining) {
        pool.submit([&, remaining] {
            completed.fetch_add(1, std::memory_order_relaxed);
            if (remaining == 0)
                return;
            for (int i = 0; i < fanout; ++i)
                submit_tree(remaining - 1);
        });
    };
    submit_tree(depth);
    pool.shutdown();
    std::size_t expected = 0;
    std::size_t level = 1;
    for (int i = 0; i <= depth; ++i) {
        expected += level;
        level *= fanout;
    }
    REQUIRE(completed.load(std::memory_order_relaxed) == expected);
}

TEST_CASE("ThreadPool survives concurrent submit and shutdown",
          "[thread_pool]") {
    ThreadPool pool(4);
    constexpr int submit_count = 100'000;
    std::atomic<bool> start{false};
    std::atomic<int> completed{0};
    std::thread submitter([&] {
        while (!start.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < submit_count; ++i) {
            try {
                pool.submit(
                    [&] { completed.fetch_add(1, std::memory_order_relaxed); });
            } catch (...) {
                // shutdown() won the race.
                break;
            }
        }
    });
    start.store(true, std::memory_order_release);
    pool.shutdown();
    submitter.join();
}

TEST_CASE("ThreadPool executes work submitted by workers", "[thread_pool]") {
    ThreadPool pool(8);
    constexpr std::size_t parent_count = 1'000;
    constexpr std::size_t children_per_parent = 10;
    std::atomic<std::size_t> parents_completed{0};
    std::atomic<std::size_t> children_completed{0};
    for (std::size_t i = 0; i < parent_count; ++i) {
        pool.submit([&] {
            for (std::size_t j = 0; j < children_per_parent; ++j) {
                pool.submit([&] {
                    children_completed.fetch_add(1, std::memory_order_relaxed);
                });
            }
            parents_completed.fetch_add(1, std::memory_order_relaxed);
        });
    }
    pool.shutdown();
    REQUIRE(parents_completed.load(std::memory_order_relaxed) == parent_count);
    REQUIRE(children_completed.load(std::memory_order_relaxed) ==
            parent_count * children_per_parent);
}

TEST_CASE("ThreadPool handles large worker-local task batches",
          "[thread_pool]") {
    ThreadPool pool(4);
    std::size_t const child_count =
        GENERATE(255, 256, 257, 511, 512, 513, 1023, 1024, 10'000);
    std::atomic<std::size_t> completed{0};
    std::latch parent_started(1);
    std::latch release_parent(1);
    pool.submit([&] {
        parent_started.count_down();
        release_parent.wait();
        for (std::size_t i = 0; i < child_count; ++i) {
            pool.submit(
                [&] { completed.fetch_add(1, std::memory_order_relaxed); });
        }
    });
    parent_started.wait();
    release_parent.count_down();
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == child_count);
}

TEST_CASE("ThreadPool survives repeated worker-local deque growth",
          "[thread_pool]") {
    for (int iteration = 0; iteration < 1'000; ++iteration) {
        ThreadPool pool(4);
        constexpr std::size_t task_count = 2'000;
        std::atomic<std::size_t> completed{0};
        std::latch start(1);
        pool.submit([&] {
            start.wait();
            for (std::size_t i = 0; i < task_count; ++i) {
                pool.submit(
                    [&] { completed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
        start.count_down();
        pool.shutdown();
        REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
    }
}

TEST_CASE("ThreadPool handles high volume of tiny tasks", "[thread_pool]") {
    ThreadPool pool(8);
    std::size_t const task_count = 1'000'000;
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool supports concurrent shutdown calls", "[thread_pool]") {
    ThreadPool pool(8);
    std::size_t const task_count = 10'000;
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    std::vector<std::thread> shutdowners;
    for (int i = 0; i < 8; ++i)
        shutdowners.emplace_back([&] { pool.shutdown(); });
    for (auto& thread : shutdowners)
        thread.join();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool allows work stealing", "[thread_pool]") {
    ThreadPool pool(4);
    constexpr std::size_t task_count = 1'000;
    std::atomic<std::size_t> completed{0};
    std::latch first_task_started(1);
    std::latch release_first_task(1);
    pool.submit([&] {
        first_task_started.count_down();
        release_first_task.wait();
    });
    first_task_started.wait();
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    // Give other workers a chance to steal.
    std::this_thread::sleep_for(10ms);
    CHECK(completed.load(std::memory_order_relaxed) < task_count);
    release_first_task.count_down();
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool handles concurrent submitters", "[thread_pool]") {
    ThreadPool pool(8);
    constexpr std::size_t submitter_count = 8;
    constexpr std::size_t tasks_per_submitter = 5'000;
    std::atomic<std::size_t> completed{0};
    std::vector<std::thread> submitters;
    submitters.reserve(submitter_count);
    for (std::size_t i = 0; i < submitter_count; ++i) {
        submitters.emplace_back([&] {
            for (std::size_t j = 0; j < tasks_per_submitter; ++j) {
                pool.submit(
                    [&] { completed.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    for (auto& submitter : submitters)
        submitter.join();
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) ==
            submitter_count * tasks_per_submitter);
}

TEST_CASE("ThreadPool executes concurrently submitted tasks exactly once",
          "[thread_pool]") {
    constexpr std::size_t submitter_count = 8;
    constexpr std::size_t tasks_per_submitter = 2'000;
    constexpr std::size_t task_count = submitter_count * tasks_per_submitter;
    ThreadPool pool(8);
    std::vector<std::atomic<int>> executions(task_count);
    for (auto& execution : executions)
        execution.store(0);
    std::vector<std::thread> submitters;
    for (std::size_t submitter = 0; submitter < submitter_count; ++submitter) {
        submitters.emplace_back([&, submitter] {
            for (std::size_t i = 0; i < tasks_per_submitter; ++i) {
                const std::size_t index = submitter * tasks_per_submitter + i;
                pool.submit([&, index] {
                    executions[index].fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }
    for (auto& submitter : submitters)
        submitter.join();
    pool.shutdown();
    for (std::size_t i = 0; i < task_count; ++i)
        REQUIRE(executions[i].load(std::memory_order_relaxed) == 1);
}

TEST_CASE("ThreadPool shutdown waits for running tasks", "[thread_pool]") {
    ThreadPool pool(4);
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    std::latch release_task(1);
    pool.submit([&] {
        started.store(true, std::memory_order_release);
        release_task.wait();
        finished.store(true, std::memory_order_release);
    });
    while (!started.load(std::memory_order_acquire))
        std::this_thread::yield();
    std::thread shutdown_thread([&] { pool.shutdown(); });
    std::this_thread::sleep_for(10ms);
    REQUIRE_FALSE(finished.load(std::memory_order_acquire));
    release_task.count_down();
    shutdown_thread.join();
    REQUIRE(finished.load(std::memory_order_acquire));
}

TEST_CASE("ThreadPool shutdown waits for queued tasks", "[thread_pool]") {
    ThreadPool pool(2);
    constexpr std::size_t task_count = 1'000;
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i) {
        pool.submit([&] {
            std::this_thread::sleep_for(1ms);
            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool shutdown waits for children submitted by running tasks",
          "[thread_pool]") {
    constexpr int parent_count = 4;
    constexpr int children_per_parent = 100;
    ThreadPool pool(parent_count);
    std::atomic<int> children_completed{0};
    std::latch parents_started(parent_count);
    std::latch release_parents(1);
    for (int i = 0; i < parent_count; ++i) {
        pool.submit([&] {
            parents_started.count_down();
            release_parents.wait();
            for (int j = 0; j < children_per_parent; ++j) {
                pool.submit([&] {
                    children_completed.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }
    parents_started.wait();
    std::thread shutdown_thread([&] { pool.shutdown(); });
    std::this_thread::sleep_for(1ms);
    release_parents.count_down();
    shutdown_thread.join();
    REQUIRE(children_completed.load(std::memory_order_relaxed) ==
            parent_count * children_per_parent);
}

TEST_CASE("ThreadPool allows worker submission during shutdown",
          "[thread_pool]") {
    ThreadPool pool(4);
    std::atomic<int> completed{0};
    std::latch started(1);
    std::latch release(1);
    pool.submit([&] {
        started.count_down();
        release.wait();
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    });
    started.wait();
    std::thread shutdown_thread([&] { pool.shutdown(); });
    std::this_thread::sleep_for(1ms);
    release.count_down();
    shutdown_thread.join();
    REQUIRE(completed.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("ThreadPool shutdown is idempotent", "[thread_pool]") {
    ThreadPool pool(4);
    std::atomic<int> completed{0};
    pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    pool.shutdown();
    pool.shutdown();
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("ThreadPool destructor waits for tasks", "[thread_pool]") {
    std::atomic<bool> completed{false};
    {
        ThreadPool pool(4);
        pool.submit([&] {
            std::this_thread::sleep_for(10ms);
            completed.store(true, std::memory_order_release);
        });
    }
    REQUIRE(completed.load(std::memory_order_acquire));
}

TEST_CASE("ThreadPool can shutdown without tasks", "[thread_pool]") {
    ThreadPool pool(8);
    pool.shutdown();
}

TEST_CASE("ThreadPool can repeatedly construct and destroy", "[thread_pool]") {
    for (int iteration = 0; iteration < 10'000; ++iteration) {
        ThreadPool pool(4);
        std::atomic<int> completed{0};
        for (int i = 0; i < 100; ++i)
            pool.submit(
                [&] { completed.fetch_add(1, std::memory_order_relaxed); });
        pool.shutdown();
        REQUIRE(completed.load(std::memory_order_relaxed) == 100);
    }
}

TEST_CASE("ThreadPool supports single worker", "[thread_pool]") {
    ThreadPool pool(1);
    constexpr std::size_t task_count = 10'000;
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool supports more workers than tasks", "[thread_pool]") {
    ThreadPool pool(64);
    constexpr std::size_t task_count = 10;
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool handles a large number of tasks", "[thread_pool]") {
    constexpr std::size_t task_count = 100'000;
    ThreadPool pool(8);
    std::atomic<std::size_t> completed{0};
    for (std::size_t i = 0; i < task_count; ++i)
        pool.submit([&] { completed.fetch_add(1, std::memory_order_relaxed); });
    pool.shutdown();
    REQUIRE(completed.load(std::memory_order_relaxed) == task_count);
}

TEST_CASE("ThreadPool handles mixed external and worker submissions",
          "[thread_pool]") {
    ThreadPool pool(8);
    constexpr std::size_t submitter_count = 4;
    constexpr std::size_t tasks_per_submitter = 2'000;
    constexpr std::size_t children_per_task = 10;
    std::atomic<std::size_t> completed{0};
    std::vector<std::thread> submitters;
    for (std::size_t i = 0; i < submitter_count; ++i) {
        submitters.emplace_back([&] {
            for (std::size_t j = 0; j < tasks_per_submitter; ++j) {
                pool.submit([&] {
                    completed.fetch_add(1, std::memory_order_relaxed);
                    for (std::size_t k = 0; k < children_per_task; ++k) {
                        pool.submit([&] {
                            completed.fetch_add(1, std::memory_order_relaxed);
                        });
                    }
                });
            }
        });
    }
    for (auto& submitter : submitters)
        submitter.join();
    pool.shutdown();
    std::size_t const expected =
        submitter_count * tasks_per_submitter * (1 + children_per_task);
    REQUIRE(completed.load(std::memory_order_relaxed) == expected);
}

TEST_CASE("ThreadPool preserves captured task state", "[thread_pool]") {
    ThreadPool pool(4);
    std::mutex mutex;
    std::vector<int> values;
    constexpr int task_count = 1'000;
    for (int i = 0; i < task_count; ++i) {
        pool.submit([&, i] {
            std::lock_guard<std::mutex> lock(mutex);
            values.push_back(i);
        });
    }
    pool.shutdown();
    REQUIRE(values.size() == static_cast<std::size_t>(task_count));
    std::sort(values.begin(), values.end());
    for (int i = 0; i < task_count; ++i)
        REQUIRE(values[i] == i);
}
