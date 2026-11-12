#pragma once

#include <memory>
#include <ranges>
#include <vector>

#include <utl/hashtable.hpp>

#include "application/window.h"

namespace mosaic {

class WindowSystem {
public:
    struct AppDelegate {
        virtual ~AppDelegate() = default;

        virtual void request_frames() = 0;
    };

    explicit WindowSystem(std::unique_ptr<AppDelegate> delegate)
        : m_app_delegate(std::move(delegate)) {}

    std::size_t num_windows() const { return m_windows.size(); }

    auto windows() {
        return m_windows | std::views::transform(
                               [](auto& p) -> Window* { return p.get(); });
    }

    auto windows() const {
        return m_windows | std::views::transform([](auto& p) -> Window const* {
                   return p.get();
               });
    }

    Window* create_window(WindowDesc const& desc);

    void queue_window_destruction(Window* window);

    void update();

private:
    struct WindowDelegate;

    bool try_destroy_window(Window* window);

    std::unique_ptr<AppDelegate> m_app_delegate;
    std::vector<std::unique_ptr<Window>> m_windows;
    utl::hashset<Window*> m_to_delete;
};

} // namespace mosaic