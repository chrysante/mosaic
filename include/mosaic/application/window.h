#pragma once

#include <memory>
#include <vector>

#include <vml/vml.hpp>

#include <mosaic/application/window_delegate.h>
#include <mosaic/application/window_desc.h>
#include <mosaic/support/export.h>

namespace mosaic {

class InputEvent;

namespace internal { struct WindowImpl; }

///
class MOSAIC_EXPORT Window {
public:
    Window(Window const&) = delete;
    Window& operator=(Window const&) = delete;

    ~Window();

    /// \returns the window title
    std::string title() const;

    /// \returns the window's position on the monitor
    vml::int2 position() const;

    /// \returns the window's size
    vml::int2 size() const;

    /// \returns true iff the user is currently resizing the window
    bool is_live_resizing() const;

    /// \returns true iff the user is currently dragging the window
    bool is_live_moving() const;

    /// \returns the height of the window's title bar
    float titlebar_height() const;

    /// \returns the bounding box of the close, minimize and maximize buttons in
    /// window coordinates
    vml::AABB<float, 2> title_buttons_area() const;

    ///
    void set_style(WindowStyle const& style);

    ///
    void zoom();

    ///
    void set_focused();

    ///
    void* native_handle();

    ///
    void add_delegate(std::unique_ptr<WindowDelegate> delegate);

private:
    friend class Application;
    friend struct internal::WindowImpl;

    Window(WindowDesc const& desc);

    void platform_constructor(WindowDesc const& desc);

    void apply_style();

    std::unique_ptr<internal::WindowImpl> m_impl;
};

} // namespace mosaic