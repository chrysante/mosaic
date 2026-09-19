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

private:
    friend std::unique_ptr<class Window> create_window(WindowDesc const&);

    Window(WindowDesc const& desc);

    struct Data;
    std::unique_ptr<Data> m_data;
};

} // namespace mosaic