#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <mosaic/support/export.h>

namespace mosaic {

namespace thread_pool_impl {
using Task = std::function<void()>;
struct Worker;
} // namespace thread_pool_impl

/// A fixed-size thread pool that executes submitted tasks concurrently.
///
/// The pool owns and manages its worker threads. Tasks may be submitted from
/// any thread, including tasks currently executing in the pool. Tasks submitted
/// by worker threads may recursively submit additional tasks.
class MOSAIC_EXPORT ThreadPool {
public:
    /// Constructs a thread pool using `std::thread::hardware_concurrency()`
    /// worker threads. If `hardware_concurrency()` returns zero, one worker
    /// thread is created.
    ThreadPool();

    /// Constructs a thread pool with thread_count worker threads. If
    /// thread_count is zero, one worker thread is created.
    explicit ThreadPool(std::size_t thread_count);

    /// Stops the pool and waits for all accepted tasks to complete.
    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Submits a task for execution.
    ///
    /// Returns true if the task was accepted. Returns false if shutdown has
    /// already started. Tasks submitted by a worker of this pool are accepted
    /// while shutdown is in progress.
    ///
    /// This function does not throw. Failure to allocate memory or otherwise
    /// satisfy an internal resource requirement terminates the process.
    bool submit(std::function<void()> task) noexcept;

    /// Stops accepting submissions from non-worker threads and waits for all
    /// accepted tasks to complete, including tasks submitted by running workers
    /// during shutdown.
    ///
    /// Safe to call multiple times.
    /// Must not be called from a worker thread.
    void shutdown() noexcept;

private:
    void enqueue_task(thread_pool_impl::Task* task);

    bool begin_submit();

    void end_submit() noexcept;

    void worker_loop(std::size_t worker_index);

    thread_pool_impl::Task* steal_task(std::size_t worker_index);

    void run_task(thread_pool_impl::Task* task) noexcept;

    bool is_worker_thread() const noexcept;

    // Bit 0:
    //   1 = accepting submissions
    //   0 = shutdown has started
    // Bits 1+:
    //   number of active submitters * 2
    std::atomic<std::uint64_t> m_lifecycle{1};
    std::atomic<std::size_t> m_pending_tasks{0};
    std::atomic<bool> m_stop{false};
    // Monotonically increasing sequence used for worker wakeups.
    // Unlike a bool "work available" flag, it doesn't suffer from the value
    // changing A -> B -> A between wait calls.
    std::atomic<std::uint64_t> m_wake_generation{0};
    std::atomic<std::size_t> m_next_worker{0};
    std::vector<std::unique_ptr<thread_pool_impl::Worker>> m_workers;
    std::vector<std::thread> m_threads;
    // Shutdown is rare, so serializing shutdown callers doesn't
    // affect the task submission hot path.
    std::mutex m_shutdown_mutex;
};

} // namespace mosaic