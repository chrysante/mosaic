#pragma once

#import <Metal/Metal.h>

#include "application/application.h"

#import <AppKit/NSApplication.h>
#import <Metal/Metal.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>

@interface DisplayLinkTarget : NSObject
@property(nonatomic, assign) mosaic::Application* application;
- (void)displayLinkCallback:(CADisplayLink*)displayLink;
@end

@implementation DisplayLinkTarget
- (void)displayLinkCallback:(CADisplayLink*)displayLink {
    mosaic::internal::run_one_frame(*_application);
}
@end

@interface MetalView : NSView
@property(nonatomic, readonly) CAMetalLayer* metal_layer;
@end

namespace mosaic {

struct Application::PlatformData {
    NSApplication* app = nil;
    CADisplayLink* display_link = nil;
    DisplayLinkTarget* display_link_target = nil;
    MetalView* metal_view;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;

    // Frame data
    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
};

} // namespace mosaic