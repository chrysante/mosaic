#include "application/window.h"

#include "application/window_desc.h"

#include <memory>

using namespace mosaic;

void internal::set_window_titlebar_controls_hovered(Window& window,
                                                    bool value) {
    window.m_titlebar_controls_hovered = value;
}

std::unique_ptr<class Window> internal::create_window(WindowDesc const& desc) {
    return std::unique_ptr<Window>(new Window(desc));
}

void Window::set_style(WindowStyle const& style) {
    if (m_style == style) return;
    m_style = style;
    apply_style();
}

void Window::add_delegate(std::unique_ptr<WindowDelegate> delegate) {
    delegate->m_window = this;
    m_delegates.push_back(std::move(delegate));
}