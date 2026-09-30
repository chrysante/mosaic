#include <atomic>
#include <cassert>
#include <cstddef>
#include <mutex>
#include <utility>

#include <mosaic/support/export.h>

namespace mosaic {

namespace dispatcher_impl {

class CompletionStatePool;

/// State stored by the dispatcher to allow waiting for jobs to finish
class CompletionState {
public:
    CompletionState() noexcept = default;

    CompletionState(const CompletionState&) = delete;
    CompletionState& operator=(const CompletionState&) = delete;

    void reset(std::size_t count) noexcept {
        assert(count != 0);
        assert(m_remaining.load(std::memory_order_relaxed) == 0);
        assert(m_references.load(std::memory_order_relaxed) == 0);
        m_remaining.store(count, std::memory_order_relaxed);
    }

    void add_reference() noexcept {
        // The caller must already own a reference to the state.
        m_references.fetch_add(1, std::memory_order_relaxed);
    }

    bool release_reference() noexcept {
        return m_references.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    bool ready() const noexcept {
        return m_remaining.load(std::memory_order_acquire) == 0;
    }

    void wait() const noexcept {
        std::size_t remaining = m_remaining.load(std::memory_order_acquire);
        while (remaining != 0) {
            m_remaining.wait(remaining, std::memory_order_relaxed);
            remaining = m_remaining.load(std::memory_order_acquire);
        }
    }

    void complete_one() noexcept {
        std::size_t const previous =
            m_remaining.fetch_sub(1, std::memory_order_acq_rel);
        assert(previous != 0);
        if (previous == 1)
            m_remaining.notify_all();
    }

private:
    std::atomic<std::size_t> m_remaining{0};
    // Keeps the state alive while either a Completion or a worker task
    // holds a reference to it.
    std::atomic<std::size_t> m_references{0};
};

/// Fixed-size object allocator for `CompletionState` objects.
class CompletionStatePool {
public:
    explicit CompletionStatePool(std::size_t states_per_page = 256) noexcept;

    ~CompletionStatePool() noexcept;

    CompletionStatePool(const CompletionStatePool&) = delete;
    CompletionStatePool& operator=(const CompletionStatePool&) = delete;

    CompletionState* acquire(std::size_t remaining) noexcept;

    void release(CompletionState* state) noexcept;

private:
    struct FreeNode {
        FreeNode* next;
    };

    static_assert(sizeof(FreeNode) <= sizeof(CompletionState));
    static_assert(alignof(FreeNode) <= alignof(CompletionState));

    struct alignas(CompletionState) Page {
        Page* next;
    };

    void allocate_page_locked() noexcept;

    const std::size_t m_states_per_page;
    std::mutex m_mutex;
    Page* m_pages = nullptr;
    FreeNode* m_free = nullptr;
    std::size_t m_outstanding = 0;
};

/// Internal RAII ownership handle of one reference to a `CompletionState`
/// object.
class CompletionStateHandle {
public:
    CompletionStateHandle() noexcept = default;

    CompletionStateHandle(const CompletionStateHandle& other) noexcept
        : m_pool(other.m_pool)
        , m_state(other.m_state) {
        if (m_state)
            m_state->add_reference();
    }

    CompletionStateHandle&
    operator=(const CompletionStateHandle& other) noexcept {
        if (this == &other)
            return *this;
        reset();
        m_pool = other.m_pool;
        m_state = other.m_state;
        if (m_state)
            m_state->add_reference();
        return *this;
    }

    CompletionStateHandle(CompletionStateHandle&& other) noexcept
        : m_pool(std::exchange(other.m_pool, nullptr))
        , m_state(std::exchange(other.m_state, nullptr)) {}

    CompletionStateHandle& operator=(CompletionStateHandle&& other) noexcept {
        if (this == &other)
            return *this;
        reset();
        m_pool = std::exchange(other.m_pool, nullptr);
        m_state = std::exchange(other.m_state, nullptr);
        return *this;
    }

    ~CompletionStateHandle() {
        reset();
    }

    void reset() noexcept {
        CompletionState* state = std::exchange(m_state, nullptr);
        CompletionStatePool* pool = std::exchange(m_pool, nullptr);
        if (state == nullptr)
            return;
        if (state->release_reference())
            pool->release(state);
    }

    CompletionState* state() const noexcept {
        return m_state;
    }

    explicit operator bool() const noexcept {
        return m_state != nullptr;
    }

    static CompletionStateHandle adopt(CompletionStatePool* pool,
                                       CompletionState* state) noexcept {
        assert(pool != nullptr);
        assert(state != nullptr);
        return CompletionStateHandle(pool, state, AdoptTag{});
    }

private:
    struct AdoptTag {};

    CompletionStateHandle(CompletionStatePool* pool, CompletionState* state,
                          AdoptTag) noexcept
        : m_pool(pool)
        , m_state(state) {}

private:
    CompletionStatePool* m_pool = nullptr;
    CompletionState* m_state = nullptr;
};

} // namespace dispatcher_impl

// ============================================================================
// Completion
// ============================================================================
class MOSAIC_EXPORT Completion {
    friend class WorkDispatcher;

public:
    Completion() noexcept = default;

    void wait() const noexcept {
        if (m_state)
            m_state.state()->wait();
    }

    bool ready() const noexcept {
        return !m_state || m_state.state()->ready();
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(m_state);
    }

private:
    explicit Completion(dispatcher_impl::CompletionStateHandle state) noexcept
        : m_state(std::move(state)) {}

    dispatcher_impl::CompletionStateHandle m_state;
};

} // namespace mosaic