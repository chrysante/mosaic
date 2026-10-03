#pragma once

#include <functional>
#include <type_traits>

#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow.h>

#include "application/window.h"
#include "application/window_impl.h"

@interface WindowDelegateProxy : NSObject <NSWindowDelegate>
@property(nonatomic, assign) mosaic::Window* window;
@end

namespace mosaic::internal {

enum class CocoaEventType : short { window_shall_zoom };

using NativeEventHandler = std::function<void(NSEvent*)>;

#define GET_WINDOW_IMPL(window)                                                \
    static_cast<std::conditional_t<                                            \
        ::std::is_const_v<::std::remove_reference_t<decltype(window)>>,        \
        ::mosaic::internal::WindowImplMacOS const&,                            \
        ::mosaic::internal::WindowImplMacOS&>>(                                \
        ::mosaic::internal::WindowImpl::get_impl(window))

#define WINDOW_IMPL GET_WINDOW_IMPL(*this)

struct WindowImplMacOS : WindowImpl {
    static void install_native_mouse_event_handler(Window&, NativeEventHandler);

    static NativeEventHandler const&
    get_native_mouse_event_handler(Window const&);

    NSWindow* native_window = nil;
    WindowDelegateProxy* delegate = nil;
    bool is_live_moving = false;
    NativeEventHandler native_mouse_event_handler = nullptr;
};

} // namespace mosaic::internal