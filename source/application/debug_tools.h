#pragma once

#include <deque>

#include "application/input_event.h"

#ifndef NDEBUG

namespace mosaic {

class Application;

template <typename T> class FixedQueue {
public:
    explicit FixedQueue(std::size_t capacity)
        : m_capacity(capacity) {}

    void push(T value) {
        if (m_data.size() == m_capacity) m_data.pop_front();
        m_data.push_back(std::move(value));
    }

    std::size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }
    void clear() noexcept { return m_data.clear(); }

    auto begin() { return m_data.begin(); }
    auto begin() const { return m_data.begin(); }

    auto end() { return m_data.end(); }
    auto end() const { return m_data.end(); }

private:
    std::deque<T> m_data;
    std::size_t m_capacity;
};

class ApplicationDebugger {
public:
    ApplicationDebugger(Application& app)
        : app(app)
        , m_input_queue(100) {}

    void display();

    void toggle_open() { m_open = !m_open; }

    void on_input(InputEventUnion event) { m_input_queue.push(event); }

private:
    void draw_window_debugger();
    void draw_event_debugger();
    void draw_input_debugger();
    void display_native_window_style_inspector();

    Application& app;
    FixedQueue<InputEventUnion> m_input_queue;
    bool m_open = false;
};

} // namespace mosaic

#endif // NDEBUG