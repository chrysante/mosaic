#include "application/window.h"

#include <memory>

#import <AppKit/NSButton.h>
#import <AppKit/NSToolbar.h>
#import <AppKit/NSWindow.h>

#include "application/input_event.h"
#include "application/window_data_macos.h"

using namespace mosaic;
using namespace vml;

struct internal::WindowImpl {
    static void for_each_delegate(Window& window, auto action) {
        for (auto& delegate : window.m_delegates)
            action(*delegate);
    }
};

using internal::WindowImpl;

// =============================================================================
// WindowDelegateProxy
// =============================================================================

@implementation WindowDelegateProxy

- (BOOL)windowShouldClose:(NSWindow*)window {
    if (!_window) return YES;
    bool allow = true;
    WindowImpl::for_each_delegate(*_window, [&](WindowDelegate& delegate) {
        allow &= delegate.should_close();
    });
    return allow;
}

- (void)windowWillMove:(NSNotification*)notification {
    if (!_window) return;
    WindowImpl::for_each_delegate(
        *_window, [](WindowDelegate& delegate) { delegate.on_move_start(); });
}

- (void)windowDidMove:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* ns_window = notification.object;
    if (ns_window != (__bridge NSWindow*)_window->native_handle()) return;
    vml::int2 position = (vml::int2)ns_window.frame.origin;
    WindowImpl::for_each_delegate(
        *_window,
        [position](WindowDelegate& delegate) { delegate.on_move(position); });
}

- (void)windowDidResize:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* ns_window = notification.object;
    if (ns_window != (__bridge NSWindow*)_window->native_handle()) return;
    NSSize size = ns_window.frame.size;
    vml::int2 new_size{(int)size.width, (int)size.height};
    WindowImpl::for_each_delegate(
        *_window,
        [new_size](WindowDelegate& delegate) { delegate.on_resize(new_size); });
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    if (!_window) return;
    WindowImpl::for_each_delegate(
        *_window, [](WindowDelegate& delegate) { delegate.on_focus(); });
}

- (void)windowDidResignKey:(NSNotification*)notification {
    if (!_window) return;
    WindowImpl::for_each_delegate(
        *_window, [](WindowDelegate& delegate) { delegate.on_focus_loss(); });
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* window = notification.object;
    CGFloat scale = window.backingScaleFactor;
    WindowImpl::for_each_delegate(*_window, [scale](WindowDelegate& delegate) {
        delegate.on_content_scale_change(scale);
    });
}

- (void)windowWillClose:(NSNotification*)notification {
    if (!_window) return;
    WindowImpl::for_each_delegate(
        *_window, [](WindowDelegate& delegate) { delegate.on_destroy(); });
}

@end

// =============================================================================
// Window implementation
// =============================================================================

Window::~Window() = default;

Window::Window(WindowDesc const& desc)
    : m_data(std::make_unique<Data>()) {
    @autoreleasepool {
        NSRect frame = NSMakeRect(desc.position.x, desc.position.y, desc.size.x,
                                  desc.size.y);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:(NSWindowStyleMaskTitled |
                                                   NSWindowStyleMaskClosable |
                                                   NSWindowStyleMaskResizable)
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        m_data->native_window = window;
        WindowDelegateProxy* delegate = [[WindowDelegateProxy alloc] init];
        delegate.window = this;
        m_data->delegate = delegate;
        window.delegate = delegate;
        [window makeKeyAndOrderFront:nil];
        set_style(desc.style);
    }
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
    @autoreleasepool {
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
}

void Window::set_focused() {
    @autoreleasepool {
        NSWindow* window = m_data->native_window;
        [NSApp activateIgnoringOtherApps:YES];
        [window makeKeyAndOrderFront:nil];
    }
}

void* Window::native_handle() { return (__bridge void*)m_data->native_window; }

static bool is_in_titlebar(Window& window, NSEvent* event) {
    return event.window.frame.size.height - event.locationInWindow.y <=
           window.titlebar_height();
}

void Window::on_input(InputEvent&, void* native_event) {
    @autoreleasepool {
        NSEvent* ns_event = (__bridge NSEvent*)native_event;
        auto* ns_window = m_data->native_window;
        if (ns_event.window != ns_window) return;
        if (ns_event.type == NSEventTypeLeftMouseDown &&
            !m_titlebar_controls_hovered && is_in_titlebar(*this, ns_event)) {
            switch (ns_event.clickCount) {
            case 1:
                [ns_window performWindowDragWithEvent:ns_event];
                break;
            case 2:
                zoom();
                break;
            default:
                break;
            }
        }
    }
}

void Window::apply_style() {
    @autoreleasepool {
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
}