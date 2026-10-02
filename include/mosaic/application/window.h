#pragma once

#include <memory>
#include <vector>

#include <vml/vml.hpp>

#include <mosaic/application/window_delegate.h>
#include <mosaic/application/window_desc.h>
#include <mosaic/support/export.h>

namespace mosaic {

class InputEvent;
class Window;

namespace internal {

struct WindowImpl;

void set_window_titlebar_controls_hovered(Window& window, bool value);

/// This is internal because all windows must be created through the application
std::unique_ptr<Window> create_window(WindowDesc const& desc);

} // namespace internal

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
    friend std::unique_ptr<class Window>
    internal::create_window(WindowDesc const&);
    friend void internal::set_window_titlebar_controls_hovered(Window& window,
                                                               bool value);

    Window(WindowDesc const& desc);

    void on_input(InputEvent& input_event, void* native_event);

    void apply_style();

    struct Data;

    std::unique_ptr<Data> m_data;
    std::vector<std::unique_ptr<WindowDelegate>> m_delegates;
    WindowStyle m_style;
    bool m_titlebar_controls_hovered = false;
};

} // namespace mosaic