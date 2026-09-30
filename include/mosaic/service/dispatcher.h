#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

#include <mosaic/service/completion.h>
#include <mosaic/service/thread_pool.h>
#include <mosaic/support/export.h>

namespace mosaic {

struct ParallelOptions {
    // Explicit number of elements processed by each task.
    //
    // Zero means automatic chunking.
    std::size_t chunk_size = 0;

    // Used when chunk_size == 0.
    //
    // The dispatcher will aim for approximately
    //
    //     worker_count * target_chunks_per_worker
    //
    // tasks.
    std::size_t target_chunks_per_worker = 4;
};

class MOSAIC_EXPORT WorkDispatcher {
    using CompletionState = dispatcher_impl::CompletionState;
    using CompletionStateHandle = dispatcher_impl::CompletionStateHandle;
    using CompletionStatePool = dispatcher_impl::CompletionStatePool;

public:
    explicit WorkDispatcher(
        ThreadPool& pool, std::size_t completion_states_per_page = 256) noexcept
        : m_pool(pool)
        , m_completion_pool(completion_states_per_page) {}

    ~WorkDispatcher() noexcept {
        shutdown();
    }

    WorkDispatcher(const WorkDispatcher&) = delete;
    WorkDispatcher& operator=(const WorkDispatcher&) = delete;

    /// Submit fire-and-forget work to the thread pool.
    ///
    /// The callable must be compatible with ThreadPool::submit(), i.e. it
    /// ultimately has to be stored in a std::function<void()>.
    ///
    /// Failure to submit is treated as a fatal internal error.
    template <std::invocable F> void dispatch(F&& function) noexcept {
        // A fire-and-forget task has nowhere to report rejection.
        // The intended use is before shutdown, so treat this as a programming
        // error rather than silently losing work.
        if (!m_pool.submit(std::forward<F>(function))) std::terminate();
    }

    /// Submit work and obtain a completion object.
    ///
    /// The returned Completion becomes ready after the callable has finished.
    template <std::invocable F>
    Completion dispatch_waitable(F&& function) noexcept {
        CompletionStateHandle completion = acquire_completion(1);
        CompletionStateHandle worker = completion;
        using Function = std::decay_t<F>;
        bool const accepted =
            m_pool.submit([fn = Function(std::forward<F>(function)),
                           worker = std::move(worker)]() mutable noexcept {
                std::invoke(fn);
                worker.state()->complete_one();
            });
        if (!accepted) {
            // No worker will ever execute the task, so we must complete the
            // state ourselves.
            completion.state()->complete_one();
            std::terminate();
        }
        return Completion(std::move(completion));
    }

    /// Queue work for execution by process_main_thread_tasks().
    ///
    /// Work submitted while process_main_thread_tasks() is executing is
    /// deferred until the next call.
    template <std::invocable F> void on_main_thread(F&& function) {
        std::lock_guard lock(m_main_mutex);
        m_main_thread_queue.emplace_back(std::forward<F>(function));
    }

    /// Queue work for the main thread and obtain a completion object.
    ///
    /// The returned Completion becomes ready after the queued function has
    /// executed.
    template <typename F> Completion on_main_thread_waitable(F&& function) {
        CompletionStateHandle completion = acquire_completion(1);
        CompletionStateHandle queued = completion;
        {
            std::lock_guard lock(m_main_mutex);
            using Function = std::decay_t<F>;
            m_main_thread_queue.emplace_back(
                [fn = Function(std::forward<F>(function)),
                 queued = std::move(queued)]() mutable noexcept {
                    std::invoke(fn);
                    queued.state()->complete_one();
                });
        }
        return Completion(std::move(completion));
    }

    /// Execute the main-thread work that was queued before this call began.
    ///
    /// Work queued by tasks executed during this call is deliberately left
    /// for the next call.
    void process_main_thread_tasks() noexcept {
        std::deque<std::function<void()>> queue;
        {
            std::lock_guard lock(m_main_mutex);
            queue = std::move(m_main_thread_queue);
        }
        for (auto& task : queue)
            task();
    }

    /// Execute function(index) for every index in [0, count).
    ///
    /// This is fire-and-forget.
    template <std::invocable<std::size_t> F>
    void parallel_for(std::size_t count, F&& function,
                      ParallelOptions options = {}) noexcept {
        if (count == 0) return;
        std::size_t const chunks = calculate_chunk_count(count, options);
        using Function = std::decay_t<F>;
        submit_parallel_chunks(count, chunks,
                               Function(std::forward<F>(function)));
    }

    /// Execute function(index) for every index in [0, count) and return a
    /// Completion which becomes ready after all chunks have finished.
    template <std::invocable<std::size_t> F>
    Completion parallel_for_waitable(std::size_t count, F&& function,
                                     ParallelOptions options = {}) {
        if (count == 0) return {};
        std::size_t const chunks = calculate_chunk_count(count, options);
        CompletionStateHandle completion = acquire_completion(chunks);
        using Function = std::decay_t<F>;
        submit_parallel_chunks(count, chunks,
                               Function(std::forward<F>(function)), completion);
        return Completion(std::move(completion));
    }

    /// \overload for ranges
    template <class Range, class F>
    void parallel_for(Range& range, F&& function,
                      ParallelOptions options = {}) noexcept {
        const std::size_t count = static_cast<std::size_t>(range.size());
        parallel_for(
            count,
            [&range, fn = std::forward<F>(function)](
                std::size_t index) mutable { fn(range[index]); },
            options);
    }

    /// \overload for ranges
    template <class Range, class F>
    Completion parallel_for_waitable(Range& range, F&& function,
                                     ParallelOptions options = {}) {
        const std::size_t count = static_cast<std::size_t>(range.size());
        return parallel_for_waitable(
            count,
            [&range, fn = std::forward<F>(function)](
                std::size_t index) mutable { fn(range[index]); },
            options);
    }

    /// Stop accepting external work and wait for all work to finish.
    ///
    /// All Completion objects obtained from this dispatcher must have been
    /// destroyed before the dispatcher itself is destroyed.
    void shutdown() noexcept {
        m_pool.shutdown();
        // At this point every worker task submitted to the pool has finished.
        //
        // There must not be any main-thread tasks still referencing
        // CompletionState objects when the dispatcher is destroyed.
        //
        // We deliberately don't process the main-thread queue here: shutdown()
        // is not a main-thread pump and must not execute application work.
    }

private:
    // Completion allocation
    CompletionStateHandle acquire_completion(std::size_t count) {
        CompletionState* state = m_completion_pool.acquire(count);
        return CompletionStateHandle::adopt(&m_completion_pool, state);
    }

    // Chunk calculation
    std::size_t calculate_chunk_count(std::size_t count,
                                      ParallelOptions options) const noexcept {
        assert(count != 0);
        if (options.chunk_size != 0)
            return (count + options.chunk_size - 1) / options.chunk_size;
        std::size_t worker_count = m_pool.worker_count();
        assert(worker_count > 0);
        std::size_t target_chunks =
            worker_count * options.target_chunks_per_worker;
        if (target_chunks == 0) target_chunks = worker_count;
        return std::min(count, target_chunks);
    }

    // Parallel submission
    template <class F, class OnComplete>
    void submit_parallel_chunks_impl(std::size_t count, std::size_t chunks,
                                     F fn, OnComplete on_complete) noexcept {
        assert(count != 0);
        assert(chunks != 0);
        assert(chunks <= count);
        for (std::size_t chunk = 0; chunk < chunks; ++chunk) {
            std::size_t const begin = (count * chunk) / chunks;
            std::size_t const end = (count * (chunk + 1)) / chunks;
            assert(begin < end);
            bool const accepted =
                m_pool.submit([fn, begin, end, on_complete]() mutable noexcept {
                    for (std::size_t i = begin; i < end; ++i)
                        std::invoke(fn, i);
                    std::invoke(on_complete);
                });
            if (!accepted) std::terminate();
        }
    }

    template <class F>
    void submit_parallel_chunks(std::size_t count, std::size_t chunks,
                                F fn) noexcept {
        submit_parallel_chunks_impl(count, chunks, std::move(fn),
                                    []() noexcept {});
    }

    template <class F>
    void submit_parallel_chunks(std::size_t count, std::size_t chunks, F fn,
                                CompletionStateHandle completion) noexcept {
        assert(completion);
        submit_parallel_chunks_impl(
            count, chunks, std::move(fn),
            [completion = std::move(completion)]() noexcept {
                completion.state()->complete_one();
            });
    }

    ThreadPool& m_pool;
    CompletionStatePool m_completion_pool;
    std::mutex m_main_mutex;
    std::deque<std::function<void()>> m_main_thread_queue;
};

} // namespace mosaic
