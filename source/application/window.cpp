#include "application/window.h"

#include "application/window_desc.h"

#include <memory>

using namespace mosaic;

std::unique_ptr<class Window> mosaic::create_window(WindowDesc const& desc) {
    auto* window = new Window(desc);
    return std::unique_ptr<Window>(window);
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