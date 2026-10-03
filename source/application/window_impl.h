#pragma once

#include <memory>
#include <string>

#include "application/window.h"

namespace mosaic {

struct internal::WindowImpl {
    static std::unique_ptr<WindowImpl> create_impl();

    static std::unique_ptr<Window> create_window(WindowDesc const& desc);

    static void set_titlebar_controls_hovered(Window& window, bool value);

    static std::string get_native_debug_string(Window const& window);

    virtual ~WindowImpl() = default;

    void for_each_delegate(auto action) const {
        for (auto& delegate : delegates)
            action(*delegate);
    }

    static WindowImpl& get_impl(Window& window) { return *window.m_impl; }

    static WindowImpl const& get_impl(Window const& window) {
        return *window.m_impl;
    }

    std::vector<std::unique_ptr<WindowDelegate>> delegates;
    WindowStyle style;
    bool titlebar_controls_hovered = false;
};

} // namespace mosaic
