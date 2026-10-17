#pragma once

#include "application/application_impl.h"

#import <AppKit/NSApplication.h>
#import <Metal/Metal.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>

#define GET_APP_IMPL(app)                                                      \
    static_cast<std::conditional_t<                                            \
        ::std::is_const_v<::std::remove_reference_t<decltype(app)>>,           \
        ::mosaic::internal::ApplicationImplMacOS const&,                       \
        ::mosaic::internal::ApplicationImplMacOS&>>(                           \
        ::mosaic::internal::ApplicationImpl::get_impl(app))

#define APP_IMPL GET_APP_IMPL(*this)

namespace mosaic::internal {

struct ApplicationImplMacOS : internal::ApplicationImpl {
    using ApplicationImpl::ApplicationImpl;

    void init_platform() override;
    int run_platform() override;
    void shutdown_platform() override;
    void create_main_window_platform(Window&) override;

    void imgui_init_platform() override;
    void imgui_begin_frame_platform() override;
    void imgui_end_frame_platform() override;

    NSApplication* native_app = nil;
    CADisplayLink* display_link = nil;
    CAMetalLayer* metal_layer = nil;

    // Persistent render data
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;

    // Frame render data
    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
};

} // namespace mosaic::internal