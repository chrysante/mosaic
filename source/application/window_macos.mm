#include "application/window.h"

#include <memory>

#import <AppKit/NSButton.h>
#import <AppKit/NSToolbar.h>
#import <AppKit/NSWindow.h>

#include "application/window_desc.h"
#include "application/window_impl_macos.h"

using namespace mosaic;
using namespace vml;

using internal::WindowImpl;
using internal::WindowImplMacOS;

// =============================================================================
// WindowImplMacOS
// =============================================================================

void WindowImplMacOS::add_native_event_handler(
    Window& window, NativeEventHandler event_handler) {
    assert(event_handler);
    GET_WINDOW_IMPL(window).native_event_handlers.push_back(
        std::move(event_handler));
}

std::unique_ptr<WindowImpl> WindowImpl::create_impl() {
    return std::make_unique<WindowImplMacOS>();
}

static void append_view_hierarchy(NSView* view, std::ostringstream& out,
                                  int depth) {
    if (!view) return;
    std::string indent(depth * 2, ' ');
    NSRect frame = view.frame;
    out << indent << NSStringFromClass(view.class).UTF8String << " frame=("
        << frame.origin.x << ", " << frame.origin.y << ", " << frame.size.width
        << ", " << frame.size.height << ")";
    if (view.hidden) out << " [hidden]";
    if (view.alphaValue != 1.0) out << " [alpha=" << view.alphaValue << "]";
    if (view.isOpaque) out << " [opaque]";
    out << "\n";
    for (NSView* subview in view.subviews)
        append_view_hierarchy(subview, out, depth + 1);
}

static std::string view_hierarchy_description(NSWindow* window) {
    std::ostringstream out;
    if (!window) return "<null window>";
    out << "Window: " << NSStringFromClass(window.class).UTF8String << "\n";
    if (window.contentView) append_view_hierarchy(window.contentView, out, 0);
    return out.str();
}

std::string WindowImpl::get_native_debug_string(Window const& window) {
    return view_hierarchy_description(GET_WINDOW_IMPL(window).native_window);
}

// =============================================================================
// WindowDelegateProxy
// =============================================================================

@implementation WindowDelegateProxy

- (BOOL)windowShouldClose:(NSWindow*)window {
    if (!_window) return YES;
    bool allow = true;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [&](WindowDelegate& delegate) { allow &= delegate.should_close(); });
    return allow;
}

- (void)windowWillMove:(NSNotification*)notification {
    if (!_window) return;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [](WindowDelegate& delegate) { delegate.on_move_start(); });
}

- (void)windowDidMove:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* ns_window = notification.object;
    if (ns_window != (__bridge NSWindow*)_window->native_handle()) return;
    vml::int2 position = (vml::int2)ns_window.frame.origin;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [position](WindowDelegate& delegate) { delegate.on_move(position); });
}

- (void)windowDidResize:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* ns_window = notification.object;
    if (ns_window != (__bridge NSWindow*)_window->native_handle()) return;
    NSSize size = ns_window.frame.size;
    vml::int2 new_size{(int)size.width, (int)size.height};
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [new_size](WindowDelegate& delegate) { delegate.on_resize(new_size); });
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    if (!_window) return;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [](WindowDelegate& delegate) { delegate.on_focus(); });
}

- (void)windowDidResignKey:(NSNotification*)notification {
    if (!_window) return;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [](WindowDelegate& delegate) { delegate.on_focus_loss(); });
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    if (!_window) return;
    NSWindow* window = notification.object;
    CGFloat scale = window.backingScaleFactor;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [scale](WindowDelegate& delegate) {
            delegate.on_content_scale_change(scale);
        });
}

- (void)windowWillClose:(NSNotification*)notification {
    if (!_window) return;
    GET_WINDOW_IMPL(*_window).for_each_delegate(
        [](WindowDelegate& delegate) { delegate.on_destroy(); });
}

@end

// =============================================================================
// Window implementation
// =============================================================================

static NSWindowStyleMask to_ns_style_mask(WindowStyle const& style) {
    NSWindowStyleMask style_mask = 0;
    if (style.decoration == WindowDecoration::none) {
        style_mask |= NSWindowStyleMaskBorderless;
    } else {
        style_mask |= NSWindowStyleMaskTitled;
        if (test(style.capabilities & WindowCapability::resizable))
            style_mask |= NSWindowStyleMaskResizable;
        if (test(style.capabilities & WindowCapability::closable))
            style_mask |= NSWindowStyleMaskClosable;
        if (test(style.capabilities & WindowCapability::minimizable))
            style_mask |= NSWindowStyleMaskMiniaturizable;
        if (style.large_titlebar)
            style_mask |= NSWindowStyleMaskFullSizeContentView;
    }
    return style_mask;
}

static bool is_in_titlebar(Window& window, NSEvent* event) {
    if (event.window.frame.size.height - event.locationInWindow.y >
        window.titlebar_height())
        return false;
    return !GET_WINDOW_IMPL(window).titlebar_controls_hovered;
}

static bool handle_window_drag_begin(Window& window, NSEvent* event) {
    auto* ns_window = GET_WINDOW_IMPL(window).native_window;
    if (event.window != ns_window) return false;
    assert(event.type == NSEventTypeLeftMouseDown);
    if (!is_in_titlebar(window, event)) return false;
    switch (event.clickCount) {
    case 1:
        [ns_window performWindowDragWithEvent:event];
        GET_WINDOW_IMPL(window).is_live_moving = true;
        return true;
    case 2:
        window.zoom();
        return true;
    default:
        return false;
    }
    return false;
}

static void handle_window_drag_end(Window& window, NSEvent* event) {
    assert(event.type == NSEventTypeLeftMouseUp);
    if (GET_WINDOW_IMPL(window).is_live_moving)
        GET_WINDOW_IMPL(window).is_live_moving = false;
}

static void forward_event(Window const* window, NSEvent* event) {
    auto& handlers = GET_WINDOW_IMPL(*window).native_event_handlers;
    for (auto& handler : handlers)
        handler(event);
}

@interface WindowContentView : NSView
@property(nonatomic, assign) mosaic::Window* mosaic_window;
@end

@implementation WindowContentView
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (void)scrollWheel:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)mouseDown:(NSEvent*)event {
    handle_window_drag_begin(*_mosaic_window, event);
    forward_event(_mosaic_window, event);
}
- (void)mouseUp:(NSEvent*)event {
    handle_window_drag_end(*_mosaic_window, event);
    forward_event(_mosaic_window, event);
}
- (void)mouseDragged:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)rightMouseDown:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)rightMouseUp:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)rightMouseDragged:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)otherMouseDown:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)otherMouseUp:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)otherMouseDragged:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)keyDown:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
- (void)keyUp:(NSEvent*)event {
    forward_event(_mosaic_window, event);
}
@end

void Window::platform_constructor(WindowDesc const& desc) {
    @autoreleasepool {
        NSRect frame = NSMakeRect(desc.position.x, desc.position.y, desc.size.x,
                                  desc.size.y);
        auto style_mask = to_ns_style_mask(desc.style);
        NSWindow* window =
            [[NSWindow alloc] initWithContentRect:frame
                                        styleMask:style_mask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
        NSRect content_rect = [window contentRectForFrameRect:frame];
        WindowContentView* content_view = [[WindowContentView alloc]
            initWithFrame:NSMakeRect(0, 0, content_rect.size.width,
                                     content_rect.size.height)];
        window.contentView = content_view;
        content_view.mosaic_window = this;
        WINDOW_IMPL.native_window = window;
        WindowDelegateProxy* delegate = [[WindowDelegateProxy alloc] init];
        delegate.window = this;
        WINDOW_IMPL.delegate = delegate;
        window.delegate = delegate;
        if (desc.visible) {
            if (desc.focused) [window makeKeyAndOrderFront:nil];
            else [window orderFront:nil];
        }
        set_style(desc.style);
    }
}

static std::string ns_string_to_std_string(NSString* str) {
    return [str UTF8String];
}

std::string Window::title() const {
    return ns_string_to_std_string(WINDOW_IMPL.native_window.title);
}

vml::int2 Window::position() const {
    return (int2)WINDOW_IMPL.native_window.frame.origin;
}

vml::int2 Window::size() const {
    auto size = WINDOW_IMPL.native_window.frame.size;
    return {(int)size.width, (int)size.height};
}

bool Window::is_live_resizing() const {
    return WINDOW_IMPL.native_window.inLiveResize;
}

bool Window::is_live_moving() const { return WINDOW_IMPL.is_live_moving; }

float Window::titlebar_height() const {
    NSWindow* window = WINDOW_IMPL.native_window;
    return (float)window.contentView.safeAreaInsets.top;
}

vml::AABB<float, 2> Window::title_buttons_area() const {
    NSWindow* window = WINDOW_IMPL.native_window;
    auto a = [window standardWindowButton:NSWindowCloseButton].frame;
    auto b = [window standardWindowButton:NSWindowMiniaturizeButton].frame;
    auto c = [window standardWindowButton:NSWindowZoomButton].frame;
    auto rect = NSUnionRect(NSUnionRect(a, b), c);
    float2 origin = {(float)rect.origin.x, (float)rect.origin.y};
    return {origin, origin + float2(rect.size.width, rect.size.height)};
}

void Window::zoom() {
    @autoreleasepool {
        [WINDOW_IMPL.native_window zoom:nil];
    }
}

void Window::set_focused() {
    @autoreleasepool {
        NSWindow* window = WINDOW_IMPL.native_window;
        [NSApp activateIgnoringOtherApps:YES];
        [window makeKeyAndOrderFront:nil];
    }
}

void* Window::native_handle() {
    return (__bridge void*)WINDOW_IMPL.native_window;
}

void Window::apply_style() {
    @autoreleasepool {
        NSWindow* window = WINDOW_IMPL.native_window;
        window.styleMask = to_ns_style_mask(m_impl->style);
        NSButton* zoom_button =
            [window standardWindowButton:NSWindowZoomButton];
        zoom_button.enabled =
            test(m_impl->style.capabilities & WindowCapability::zoomable);
        window.movable = false;
        if (m_impl->style.full_size_content) {
            window.titlebarAppearsTransparent = YES;
            window.titleVisibility = NSWindowTitleHidden;
        }
        if (m_impl->style.large_titlebar) {
            window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
            window.toolbar = [[NSToolbar alloc] init];
        }
    }
}