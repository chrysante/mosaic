#include "application/window_system.h"

#include <algorithm>
#include <cassert>

#include "application/input_internal.h"
#include "application/window_delegate.h"
#include "application/window_impl.h"

using namespace mosaic;

struct WindowSystem::WindowDelegate : mosaic::WindowDelegate {
    explicit WindowDelegate(WindowSystem* window_system)
        : window_system(window_system) {}

    void request_frames() {
        if (auto* del = window_system->m_app_delegate.get())
            del->request_frames();
    }

    void on_move(vml::int2) override { request_frames(); }
    void on_resize(vml::int2) override { request_frames(); }
    void on_focus() override { request_frames(); }
    void on_focus_loss() override { request_frames(); }
    bool should_close() override {
        request_frames();
        return true;
    }
    void on_destroy() override {
        request_frames();
        window_system->queue_window_destruction(window());
    }
    void on_content_scale_change(vml::float2) override { request_frames(); }

    WindowSystem* window_system;
};

Window* WindowSystem::create_window(WindowDesc const& desc) {
    auto window = internal::WindowImpl::create_window(desc);
    auto* result = window.get();
    m_windows.push_back(std::move(window));
    result->add_delegate(std::make_unique<WindowDelegate>(this));
    register_window_for_input(*result);
    return result;
}

void WindowSystem::destroy_window(Window* window) {
    auto& window_impl = GET_WINDOW_IMPL(*window);
    window_impl.close();
    window_impl.for_each_delegate(
        [&](mosaic::WindowDelegate& del) { del.on_destroy(); });
}

void WindowSystem::update() {
    for (auto* window : m_to_delete) {
        auto itr = std::ranges::find_if(
            m_windows, [&](auto& ptr) { return ptr.get() == window; });
        assert(itr != m_windows.end());
        m_windows.erase(itr);
    }
    m_to_delete.clear();
}

void WindowSystem::queue_window_destruction(Window* window) {
    m_to_delete.insert(window);
}