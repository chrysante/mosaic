#include "application/window.h"

#include "application/window_desc.h"
#include "application/window_impl.h"

#include <memory>

using namespace mosaic;

using internal::WindowImpl;

std::unique_ptr<Window> WindowImpl::create_window(WindowDesc const& desc) {
    return std::unique_ptr<Window>(new Window(desc));
}

void WindowImpl::set_titlebar_controls_hovered(Window& window, bool value) {
    window.m_impl->titlebar_controls_hovered = value;
}

Window::Window(WindowDesc const& desc)
    : m_impl(WindowImpl::create_impl()) {
    platform_constructor(desc);
}

Window::~Window() = default;

void Window::set_style(WindowStyle const& style) {
    if (m_impl->style == style) return;
    m_impl->style = style;
    apply_style();
}

void Window::add_delegate(std::unique_ptr<WindowDelegate> delegate) {
    delegate->m_window = this;
    m_impl->delegates.push_back(std::move(delegate));
}
