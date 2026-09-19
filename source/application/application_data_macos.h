#pragma once

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

namespace mosaic {

struct Application::PlatformData {
    NSApplication* app = nil;
    CADisplayLink* display_link = nil;
    DisplayLinkTarget* display_link_target = nil;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;

    // Frame data
    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
};

} // namespace mosaic