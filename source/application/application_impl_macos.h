#pragma once

#include "application/application_impl.h"

#import <AppKit/NSApplication.h>
#import <Metal/Metal.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>

#undef APPLICATION_IMPL_TYPE
#define APPLICATION_IMPL_TYPE ApplicationImplMacOS

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