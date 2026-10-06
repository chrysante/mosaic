#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#import <AppKit/Appkit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "application/window.h"
#include "application/window_impl.h"

#define GET_WINDOW_IMPL(window)                                                \
    static_cast<std::conditional_t<                                            \
        ::std::is_const_v<::std::remove_reference_t<decltype(window)>>,        \
        ::mosaic::internal::WindowImplMacOS const&,                            \
        ::mosaic::internal::WindowImplMacOS&>>(                                \
        ::mosaic::internal::WindowImpl::get_impl(window))

#define WINDOW_IMPL GET_WINDOW_IMPL(*this)

@interface WindowDelegateProxy : NSObject <NSWindowDelegate>
@property(nonatomic, assign) mosaic::Window* window;
@end

@interface MosaicKeyEventResponder : NSView <NSTextInputClient>
@end

@interface MosaicWindow : NSWindow
@property(nonatomic, assign) MosaicKeyEventResponder* key_event_responder;
@property(nonatomic, strong) NSTextInputContext* text_input_context;
@end

namespace mosaic::internal {

enum class CocoaEventType : short { window_shall_zoom };

using NativeEventHandler = std::function<void(NSEvent*)>;

struct WindowImplMacOS : WindowImpl {
    static void add_native_event_handler(Window&, NativeEventHandler);

    static CAMetalLayer* create_metal_layer(id<MTLDevice> device,
                                            Window& window);

    MosaicWindow* native_window = nil;
    WindowDelegateProxy* delegate = nil;
    bool is_live_moving = false;
    std::vector<NativeEventHandler> native_event_handlers;
};

} // namespace mosaic::internal