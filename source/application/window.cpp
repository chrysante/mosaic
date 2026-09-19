#include "application/window.h"

#include "application/window_desc.h"

#include <memory>

using namespace mosaic;

std::unique_ptr<class Window> mosaic::create_window(WindowDesc const& desc) {
    auto* window = new Window(desc);
    return std::unique_ptr<Window>(window);
}