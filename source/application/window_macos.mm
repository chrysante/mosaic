#include "application/window.h"

#include <memory>

#import <AppKit/NSButton.h>
#import <AppKit/NSToolbar.h>
#import <AppKit/NSWindow.h>

#include "application/input_event.h"
#include "application/window_data_macos.h"

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
    [window makeKeyAndOrderFront:nil];
    set_style(desc.style);
}

static std::string ns_string_to_std_string(NSString* str) {
    return [str UTF8String];
}

std::string Window::title() const {
    return ns_string_to_std_string(m_data->native_window.title);
}

vml::int2 Window::position() const {
    return (int2)m_data->native_window.frame.origin;
}

vml::int2 Window::size() const {
    auto size = m_data->native_window.frame.size;
    return {(int)size.width, (int)size.height};
}

float Window::titlebar_height() const {
    NSWindow* window = m_data->native_window;
    return (float)window.contentView.safeAreaInsets.top;
}

vml::AABB<float, 2> Window::title_buttons_area() const {
    NSWindow* window = m_data->native_window;
    auto a = [window standardWindowButton:NSWindowCloseButton].frame;
    auto b = [window standardWindowButton:NSWindowMiniaturizeButton].frame;
    auto c = [window standardWindowButton:NSWindowZoomButton].frame;
    auto rect = NSUnionRect(NSUnionRect(a, b), c);
    float2 origin = {(float)rect.origin.x, (float)rect.origin.y};
    return {origin, origin + float2(rect.size.width, rect.size.height)};
}

void Window::zoom() {
    NSWindow* window = m_data->native_window;
    NSEvent* event =
        [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                           location:NSMakePoint(0, 0)
                      modifierFlags:0
                          timestamp:0
                       windowNumber:window.windowNumber
                            context:nil
                            subtype:(short)CocoaEventType::window_shall_zoom
                              data1:0
                              data2:0];
    [NSApp postEvent:event atStart:YES];
}

static bool is_in_toolbar(Window& window, NSEvent* event) {
    return event.window.frame.size.height - event.locationInWindow.y <=
           window.titlebar_height();
}

void Window::on_input(InputEvent&, void* native_event) {
    NSEvent* ns_event = (__bridge NSEvent*)native_event;
    auto* ns_window = m_data->native_window;
    if (ns_event.window != ns_window) return;
    if (ns_event.type == NSEventTypeLeftMouseDown && movable &&
        is_in_toolbar(*this, ns_event)) {
        switch (ns_event.clickCount) {
        case 1:
            moving = true;
            [ns_window performWindowDragWithEvent:ns_event];
            break;
        case 2:
            zoom();
            break;
        default:
            break;
        }
    }
    if (ns_event.type == NSEventTypeLeftMouseUp) moving = false;
}

void Window::apply_style() {
    NSWindow* window = m_data->native_window;
    window.movable = false;
    if (m_style.no_decoration) {
        window.styleMask |= NSWindowStyleMaskBorderless;
        window.styleMask &= ~NSWindowStyleMaskTitled;
    } else {
        window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        window.titlebarAppearsTransparent = true;
    }
    if (m_style.has_toolbar_area) {
        window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
        window.toolbar = [[NSToolbar alloc] init];
    }
}