#include "service/thread_pool.h"

#include <new>

using namespace mosaic;
using namespace mosaic::thread_pool_impl;

namespace {

class WorkStealingDeque {
public:
    explicit WorkStealingDeque(std::size_t initial_capacity = 256);

    WorkStealingDeque(const WorkStealingDeque&) = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

    // Owner only.
    void push(Task* task);

    // Owner only.
    Task* pop();

    // Any worker may call this.
    Task* steal();

private:
    struct Buffer {
        explicit Buffer(std::size_t capacity);

        std::size_t m_capacity;
        std::size_t m_mask;
        std::unique_ptr<std::atomic<Task*>[]> m_slots;
    };

    struct alignas(std::hardware_destructive_interference_size) AtomicIndex {
        std::atomic<std::int64_t> value{0};
    };

    void grow(Buffer* buffer, std::int64_t top, std::int64_t bottom);

    AtomicIndex m_top;
    AtomicIndex m_bottom;
    // Only the owner modifies this.
    Buffer* m_buffer;
    // Stealers read this.
    std::atomic<Buffer*> m_array;
    // Old buffers are retained until the deque is destroyed.
    // This avoids reclamation while another worker might still be using
    // a buffer that it loaded before a resize.
    std::vector<std::unique_ptr<Buffer>> m_old_buffers;
    // Owner-only optimization.
    std::int64_t m_cached_top;
};

} // namespace

WorkStealingDeque::WorkStealingDeque(std::size_t initial_capacity)
    : m_buffer(nullptr)
    , m_array(nullptr)
    , m_cached_top(0) {
    if (initial_capacity < 2 ||
        (initial_capacity & (initial_capacity - 1)) != 0) {
        throw std::invalid_argument(
            "WorkStealingDeque capacity must be a power of two >= 2");
    }
    auto buffer = std::make_unique<Buffer>(initial_capacity);
    m_buffer = buffer.get();
    m_array.store(m_buffer, std::memory_order_relaxed);
    m_old_buffers.push_back(std::move(buffer));
}

void WorkStealingDeque::push(Task* task) {
    std::int64_t const bottom = m_bottom.value.load(std::memory_order_relaxed);
    Buffer* buffer = m_buffer;
    // m_cached_top is an owner-only cached upper bound on the real top.
    // Since top only increases, a stale value can only make us resize earlier,
    // never later.
    if (static_cast<std::uint64_t>(bottom - m_cached_top + 1) >
        buffer->m_capacity) {
        std::int64_t const top = m_top.value.load(std::memory_order_acquire);
        m_cached_top = top;
        if (static_cast<std::uint64_t>(bottom - top + 1) > buffer->m_capacity) {
            grow(buffer, top, bottom);
            buffer = m_buffer;
        }
    }
    buffer->m_slots[static_cast<std::size_t>(bottom) & buffer->m_mask].store(
        task, std::memory_order_relaxed);
    // Publish the task before publishing the new bottom.
    std::atomic_thread_fence(std::memory_order_release);
    m_bottom.value.store(bottom + 1, std::memory_order_release);
}

Task* WorkStealingDeque::pop() {
    std::int64_t bottom = m_bottom.value.load(std::memory_order_relaxed);
    if (bottom == 0) return nullptr;
    --bottom;
    Buffer* buffer = m_buffer;
    m_bottom.value.store(bottom, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t top = m_top.value.load(std::memory_order_relaxed);
    if (top <= bottom) {
        Task* task =
            buffer->m_slots[static_cast<std::size_t>(bottom) & buffer->m_mask]
                .load(std::memory_order_relaxed);
        if (top == bottom) {
            if (!m_top.value.compare_exchange_strong(top, top + 1,
                                                     std::memory_order_seq_cst,
                                                     std::memory_order_relaxed))
                task = nullptr;
            m_bottom.value.store(bottom + 1, std::memory_order_relaxed);
        }
        return task;
    }
    m_bottom.value.store(bottom + 1, std::memory_order_relaxed);
    return nullptr;
}

Task* WorkStealingDeque::steal() {
    std::int64_t top = m_top.value.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::int64_t const bottom = m_bottom.value.load(std::memory_order_acquire);
    if (top >= bottom) return nullptr;
    Buffer* buffer = m_array.load(std::memory_order_acquire);
    Task* task =
        buffer->m_slots[static_cast<std::size_t>(top) & buffer->m_mask].load(
            std::memory_order_relaxed);
    if (!m_top.value.compare_exchange_strong(
            top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
        return nullptr;
    return task;
}

void WorkStealingDeque::grow(Buffer* buffer, std::int64_t top,
                             std::int64_t bottom) {
    std::size_t const new_capacity = buffer->m_capacity * 2;
    auto new_buffer = std::make_unique<Buffer>(new_capacity);
    for (std::int64_t i = top; i < bottom; ++i) {
        Task* task =
            buffer->m_slots[static_cast<std::size_t>(i) & buffer->m_mask].load(
                std::memory_order_relaxed);
        new_buffer->m_slots[static_cast<std::size_t>(i) & new_buffer->m_mask]
            .store(task, std::memory_order_relaxed);
    }
    // The old buffer must remain alive because a thief may have loaded it
    // immediately before this resize.
    Buffer* new_buffer_ptr = new_buffer.get();
    m_old_buffers.push_back(std::move(new_buffer));
    // Publish the fully initialized buffer only after all its slots are ready.
    m_array.store(new_buffer_ptr, std::memory_order_release);
    // The owner uses the new buffer directly from now on.
    m_buffer = new_buffer_ptr;
}

WorkStealingDeque::Buffer::Buffer(std::size_t capacity)
    : m_capacity(capacity)
    , m_mask(capacity - 1)
    , m_slots(std::make_unique<std::atomic<Task*>[]>(capacity)) {
    for (std::size_t i = 0; i < capacity; ++i)
        m_slots[i].store(nullptr, std::memory_order_relaxed);
}

struct thread_pool_impl::Worker {
    explicit Worker(std::uint64_t seed)
        : random_state(seed) {
        inbox.reserve(32);
    }

    WorkStealingDeque deque;
    // External submissions cannot push directly into deque because
    // only this worker is allowed to own its push/pop side.
    std::mutex inbox_mutex;
    std::vector<Task*> inbox;
    std::uint64_t random_state = 0;
};

thread_local ThreadPool* current_pool = nullptr;
thread_local std::size_t current_worker = 0;

ThreadPool::ThreadPool()
    : ThreadPool(std::thread::hardware_concurrency()) {}

ThreadPool::ThreadPool(std::size_t thread_count) {
    if (thread_count == 0) thread_count = 1;
    m_workers.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i)
        m_workers.push_back(std::make_unique<Worker>(
            0x9e3779b97f4a7c15ULL + i * 0x6a09e667f3bcc909ULL));
    m_threads.reserve(thread_count);
    try {
        for (std::size_t i = 0; i < thread_count; ++i)
            m_threads.emplace_back([this, i] { worker_loop(i); });
    } catch (...) {
        m_stop.store(true, std::memory_order_release);
        m_wake_generation.fetch_add(1, std::memory_order_release);
        m_wake_generation.notify_all();
        for (std::thread& thread : m_threads)
            if (thread.joinable()) thread.join();
        throw;
    }
}

ThreadPool::~ThreadPool() noexcept {
    shutdown();
}

bool ThreadPool::submit(std::function<void()> task) noexcept {
    bool const is_worker = current_pool == this;
    if (!is_worker)
        if (!begin_submit()) return false;
    auto owned_task = new Task(std::move(task));
    m_pending_tasks.fetch_add(1, std::memory_order_acq_rel);
    try {
        enqueue_task(owned_task);
        m_wake_generation.fetch_add(1, std::memory_order_release);
        m_wake_generation.notify_one();
    } catch (...) {
        m_pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
        m_pending_tasks.notify_all();
        if (!is_worker) end_submit();
        delete owned_task;
        return false;
    }
    if (!is_worker) end_submit();
    return true;
}

void ThreadPool::shutdown() noexcept {
    if (is_worker_thread()) std::terminate();
    std::lock_guard<std::mutex> lock(m_shutdown_mutex);
    if (m_stop.load(std::memory_order_acquire)) return;
    // Stop accepting new submissions from outside the pool.
    m_lifecycle.fetch_and(~std::uint64_t{1}, std::memory_order_acq_rel);
    // Wait for external submit() calls that had already started when shutdown
    // began.
    while (true) {
        std::uint64_t const state = m_lifecycle.load(std::memory_order_acquire);
        if ((state & ~std::uint64_t{1}) == 0) break;
        m_lifecycle.wait(state, std::memory_order_acquire);
    }
    // Workers are still running here.
    // Tasks currently executing are allowed to submit more tasks. Those child
    // tasks increase m_pending_tasks and therefore keep this wait alive.
    while (true) {
        std::size_t const pending =
            m_pending_tasks.load(std::memory_order_acquire);
        if (pending == 0) break;
        m_pending_tasks.wait(pending, std::memory_order_acquire);
    }
    // There is no work left and no external submitter can introduce more work.
    m_stop.store(true, std::memory_order_release);
    m_wake_generation.fetch_add(1, std::memory_order_release);
    m_wake_generation.notify_all();
    for (std::thread& thread : m_threads)
        if (thread.joinable()) thread.join();
}

std::size_t ThreadPool::worker_count() const {
    return m_threads.size();
}

void ThreadPool::enqueue_task(Task* task) {
    if (current_pool == this) {
        std::size_t const worker_index = current_worker;
        m_workers[worker_index]->deque.push(task);
    } else {
        std::size_t const worker_index =
            m_next_worker.fetch_add(1, std::memory_order_relaxed) %
            m_workers.size();
        Worker& worker = *m_workers[worker_index];
        std::lock_guard<std::mutex> lock(worker.inbox_mutex);
        worker.inbox.push_back(task);
    }
}

bool ThreadPool::begin_submit() {
    std::uint64_t state = m_lifecycle.load(std::memory_order_acquire);
    while (true) {
        if ((state & 1) == 0) return false;
        std::uint64_t const next = state + 2;
        if (m_lifecycle.compare_exchange_weak(state, next,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed))
            return true;
    }
}

void ThreadPool::end_submit() noexcept {
    std::uint64_t const previous =
        m_lifecycle.fetch_sub(2, std::memory_order_release);
    // If shutdown has started, wake the shutdown waiter
    // whenever the active-submitter count changes.
    if ((previous & 1) == 0) m_lifecycle.notify_all();
}

static void drain_inbox(Worker& worker, std::vector<Task*>& batch) {
    batch.clear();
    {
        std::lock_guard<std::mutex> lock(worker.inbox_mutex);
        if (worker.inbox.empty()) return;
        batch.swap(worker.inbox);
    }
    for (Task* task : batch)
        worker.deque.push(task);
    batch.clear();
}

void ThreadPool::worker_loop(std::size_t worker_index) {
    current_pool = this;
    current_worker = worker_index;
    std::vector<Task*> inbox_batch;
    inbox_batch.reserve(32);
    Worker& worker = *m_workers[worker_index];
    while (true) {
        std::uint64_t const generation =
            m_wake_generation.load(std::memory_order_acquire);
        drain_inbox(worker, inbox_batch);
        if (Task* task = worker.deque.pop()) {
            run_task(task);
            continue;
        }
        if (Task* task = steal_task(worker_index)) {
            run_task(task);
            continue;
        }
        drain_inbox(worker, inbox_batch);
        if (Task* task = worker.deque.pop()) {
            run_task(task);
            continue;
        }
        if (Task* task = steal_task(worker_index)) {
            run_task(task);
            continue;
        }
        if (m_stop.load(std::memory_order_acquire)) break;
        m_wake_generation.wait(generation, std::memory_order_acquire);
    }
    while (true) {
        drain_inbox(worker, inbox_batch);
        if (Task* task = worker.deque.pop()) {
            run_task(task);
            continue;
        }
        if (Task* task = steal_task(worker_index)) {
            run_task(task);
            continue;
        }
        break;
    }
    current_pool = nullptr;
}

static std::uint64_t shuffle_random_state(std::uint64_t state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

Task* ThreadPool::steal_task(std::size_t worker_index) {
    std::size_t const worker_count = m_workers.size();
    if (worker_count <= 1) return nullptr;
    Worker& worker = *m_workers[worker_index];
    // xorshift64
    std::uint64_t x = shuffle_random_state(worker.random_state);
    worker.random_state = x;
    std::size_t victim = static_cast<std::size_t>(x % worker_count);
    // Try every worker, starting from a random victim.
    // This avoids pathological synchronized stealing.
    for (std::size_t i = 0; i < worker_count; ++i) {
        std::size_t const index = (victim + i) % worker_count;
        if (index == worker_index) continue;
        if (Task* task = m_workers[index]->deque.steal()) return task;
    }
    return nullptr;
}

void ThreadPool::run_task(Task* task) noexcept {
    (*task)();
    delete task;
    m_pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
    m_pending_tasks.notify_all();
}

bool ThreadPool::is_worker_thread() const noexcept {
    return current_pool == this;
}