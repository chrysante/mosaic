#include "service/completion.h"

#include <exception>
#include <memory>
#include <new>

using namespace mosaic;
using namespace dispatcher_impl;

// -----------------------------------------------------------------------------
// CompletionStatePool
// -----------------------------------------------------------------------------

CompletionStatePool::CompletionStatePool(std::size_t states_per_page) noexcept
    : m_states_per_page(states_per_page) {
    assert(states_per_page != 0);
}

CompletionStatePool::~CompletionStatePool() noexcept {
    assert(m_outstanding == 0);
    Page* page = m_pages;
    while (page != nullptr) {
        Page* next = page->next;
        ::operator delete(page, std::align_val_t(alignof(Page)));
        page = next;
    }
}

CompletionState* CompletionStatePool::acquire(std::size_t remaining) noexcept {
    assert(remaining != 0);
    std::lock_guard lock(m_mutex);
    if (m_free == nullptr) allocate_page_locked();
    FreeNode* node = m_free;
    m_free = node->next;
    auto* state = reinterpret_cast<CompletionState*>(node);
    std::construct_at(state);
    state->reset(remaining);
    // The caller receives ownership of one reference.
    state->add_reference();
    ++m_outstanding;
    return state;
}

void CompletionStatePool::release(CompletionState* state) noexcept {
    assert(state != nullptr);
    std::destroy_at(state);
    auto* node = reinterpret_cast<FreeNode*>(state);
    std::lock_guard lock(m_mutex);
    node->next = m_free;
    m_free = node;
    assert(m_outstanding != 0);
    --m_outstanding;
}

static std::size_t align_up(std::size_t value, std::size_t alignment) {
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

void CompletionStatePool::allocate_page_locked() noexcept {
    assert(m_free == nullptr);
    std::size_t const storage_offset =
        align_up(sizeof(Page), alignof(CompletionState));
    std::size_t const max = std::numeric_limits<std::size_t>::max();
    if (m_states_per_page > (max - storage_offset) / sizeof(CompletionState))
        std::terminate();
    std::size_t const bytes =
        storage_offset + m_states_per_page * sizeof(CompletionState);
    void* memory = ::operator new(bytes, std::align_val_t(alignof(Page)));
    auto* page = static_cast<Page*>(memory);
    page->next = m_pages;
    m_pages = page;
    auto* storage = reinterpret_cast<std::byte*>(memory) + storage_offset;
    for (std::size_t i = 0; i < m_states_per_page; ++i) {
        auto* node =
            reinterpret_cast<FreeNode*>(storage + i * sizeof(CompletionState));
        node->next = m_free;
        m_free = node;
    }
}
