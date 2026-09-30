#include "application/window.h"
#include "application/window_data_macos.h"

#import <AppKit/NSWindow.h>
#include <memory>

using namespace mosaic;
using namespace vml;

Window::~Window() = default;

Window::Window(WindowDesc const& desc)
    : m_data(std::make_unique<Data>()) {
    NSRect frame =
        NSMakeRect(desc.position.x, desc.position.y, desc.size.x, desc.size.y);
    NSWindow* window = m_data->native_window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:(NSWindowStyleMaskTitled |
                                               NSWindowStyleMaskClosable |
                                               NSWindowStyleMaskResizable)
                                      backing:NSBackingStoreBuffered
                                        defer:NO];

    [window setTitle:@"My Window"];
    [window makeKeyAndOrderFront:nil];
}

static std::string ns_string_to_std_string(NSString* str) {
    return [str UTF8String];
}

/// \returns the window title
std::string Window::title() const {
    return ns_string_to_std_string(m_data->native_window.title);
}

/// \returns the window's position on the monitor
vml::int2 Window::position() const {
    auto origin = m_data->native_window.frame.origin;
    return {(int)origin.x, (int)origin.y};
}

/// \returns the window's size
vml::int2 Window::size() const {
    auto size = m_data->native_window.frame.size;
    return {(int)size.width, (int)size.height};
}