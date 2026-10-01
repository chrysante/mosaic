#pragma once

#include <memory>
#include <mosaic/application/window_desc.h>
#include <vml/vml.hpp>

namespace mosaic {

class InputEvent;

///
std::unique_ptr<class Window> create_window(WindowDesc const& desc);

///
class Window {
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

    /// \returns the height of the window's title bar
    float titlebar_height() const;

    /// \returns the bounding box of the close, minimize and maximize buttons in
    /// window coordinates
    vml::AABB<float, 2> title_buttons_area() const;

    ///
    void set_style(WindowStyle const& style);

    ///
    void set_movable(bool value) {
        movable = value;
    }

    ///
    void zoom();

    ///
    void set_focused();

private:
    friend class Application;
    friend std::unique_ptr<class Window> create_window(WindowDesc const&);

    Window(WindowDesc const& desc);

    void on_input(InputEvent& input_event, void* native_event);

    void apply_style();

    struct Data;
    std::unique_ptr<Data> m_data;
    WindowStyle m_style;
    bool movable = false;
    bool moving = false;
};

} // namespace mosaic