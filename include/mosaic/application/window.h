#pragma once

#include <memory>
#include <mosaic/application/window_desc.h>
#include <vml/vml.hpp>

namespace mosaic {

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

private:
    friend class Application;
    friend std::unique_ptr<class Window> create_window(WindowDesc const&);

    Window(WindowDesc const& desc);

    struct Data;
    std::unique_ptr<Data> m_data;
};

} // namespace mosaic