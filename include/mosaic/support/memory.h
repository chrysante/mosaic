#pragma once

#include <functional>
#include <memory>

namespace mosaic {

template <typename T> using Owner = std::unique_ptr<T>;

template <typename T, typename... Args> Owner<T> allocate(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T> class Handle {
public:
    Handle() = default;
    Handle(Handle&&) = default;
    Handle& operator=(Handle&&) = default;
    ~Handle() noexcept { destroy(); }

    T* get() const { return m_control_block ? m_control_block->ptr : nullptr; }

    T* operator->() const { return get(); }

    explicit operator bool() const { return get() != nullptr; }

    void destroy() noexcept {
        if (m_control_block && m_control_block->ptr)
            m_control_block->destroy(m_control_block->ptr);
    }

private:
    template <typename U>
    friend Handle<U> make_handle(U* ptr, std::function<void(U*)> destroy);

    struct ControlBlock {
        explicit ControlBlock(T* ptr, std::function<void(T*)>&& destroy)
            : ptr(ptr)
            , destroy(std::move(destroy)) {}

        T* ptr = nullptr;
        std::function<void(T*)> destroy;
    };

    explicit Handle(std::shared_ptr<ControlBlock> c)
        : m_control_block(std::move(c)) {}

    std::shared_ptr<ControlBlock> m_control_block;
};

template <typename T>
Handle<T> make_handle(T* ptr, std::function<void(T*)> destroy) {
    return Handle<T>(std::make_shared<typename Handle<T>::ControlBlock>(
        ptr, std::move(destroy)));
}

} // namespace mosaic