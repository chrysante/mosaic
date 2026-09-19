#include "application/window.h"

#include <AppKit/NSWindow.h>
#include <memory>

using namespace mosaic;
using namespace vml;

struct Window::Data {
    NSWindow* native_window = nil;
};

Window::~Window() = default;

Window::Window(WindowDesc const& desc)
    : m_data(std::make_unique<Data>()) {
    NSRect frame = NSMakeRect(desc.position.x, desc.position.y, desc.size.x, desc.size.y);
    NSWindow* window = m_data->native_window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                               NSWindowStyleMaskResizable)
                                      backing:NSBackingStoreBuffered
                                        defer:NO];

    [window setTitle:@"My Window"];
    [window makeKeyAndOrderFront:nil];
}
