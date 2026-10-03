#include "application/application.h"

#include <cstdlib>
#include <iostream>
#include <memory>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

#include "application/application_data.h"
#include "application/application_data_macos.h"
#include "application/window.h"
#include "application/window_impl_macos.h"
#include "imgui/imgui_impl_mosaic_macos.h"
#include "imgui/imgui_impl_mosaic_metal.h"

using namespace mosaic;

Application::PlatformData* Application::create_platform_data() {
    return new PlatformData{};
}

void Application::destroy_platform_data(PlatformData* data) { delete data; }

void Application::platform_init() {
    @autoreleasepool {
        NSApplication* app = m_platform_data->app =
            [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app activateIgnoringOtherApps:YES];

        m_platform_data->device = MTLCreateSystemDefaultDevice();
        if (!m_platform_data->device) {
            // Metal isn't available
            std::cerr << "Metal is not available\n";
            std::exit(EXIT_FAILURE);
        }
        m_platform_data->command_queue =
            [m_platform_data->device newCommandQueue];
    }
}

void Application::platform_run() {
    NSWindow* window = GET_WINDOW_IMPL(*main_window()).native_window;
    NSView* view = window.contentView;
    DisplayLinkTarget* target = [[DisplayLinkTarget alloc] init];
    target.application = this;
    m_platform_data->display_link_target = target;
    m_platform_data->display_link =
        [view displayLinkWithTarget:target
                           selector:@selector(displayLinkCallback:)];
    [m_platform_data->display_link addToRunLoop:[NSRunLoop currentRunLoop]
                                        forMode:NSRunLoopCommonModes];
    [m_platform_data->app run];
}

void Application::platform_shutdown() { ImGui_Impl_Mosaic_MacOS_Shutdown(); }

@implementation MetalView

- (instancetype)init_with_frame:(NSRect)frame device:(id<MTLDevice>)device {
    self = [super initWithFrame:frame];
    if (self) {
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        self.wantsLayer = YES;
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.device = device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer.framebufferOnly = YES;
        layer.contentsGravity = kCAGravityTopLeft;
        self.layer = layer;
    }
    return self;
}

- (CAMetalLayer*)metal_layer {
    return (CAMetalLayer*)self.layer;
}

- (void)layout {
    [super layout];
    [self updateDrawableSize];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self updateDrawableSize];
}

- (void)updateDrawableSize {
    CAMetalLayer* layer = self.metal_layer;
    CGFloat scale = self.window.backingScaleFactor;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(self.bounds.size.width * scale,
                                    self.bounds.size.height * scale);
}

- (NSView*)hitTest:(NSPoint)point {
    return nil;
}

@end

void Application::create_main_window_platform(Window* window) {
    NSWindow* native_window = GET_WINDOW_IMPL(*window).native_window;
    NSView* content_view = native_window.contentView;
    MetalView* metal_view =
        [[MetalView alloc] init_with_frame:content_view.bounds
                                    device:m_platform_data->device];
    [content_view addSubview:metal_view
                  positioned:NSWindowAbove
                  relativeTo:nil];
    m_platform_data->metal_view = metal_view;
}

void Application::imgui_init_platform(Window* /* main_window */) {
    ImGui_Impl_Mosaic_MacOS_Init(*this);
    ImGui_Impl_Mosaic_Metal_Init(m_platform_data->device);
}

void Application::imgui_begin_frame_platform() {
    CAMetalLayer* layer = m_platform_data->metal_view.metal_layer;
    m_platform_data->drawable = [layer nextDrawable];
    if (!m_platform_data->drawable) {
        std::cerr << "Error: Have no drawable\n";
        return;
    }

    MTLRenderPassDescriptor* pass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = m_platform_data->drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

    m_platform_data->command_buffer =
        [m_platform_data->command_queue commandBuffer];

    ImGui_Impl_Mosaic_Metal_NewFrame(pass);
    ImGui_Impl_Mosaic_MacOS_NewFrame(*main_window());

    m_platform_data->encoder = [m_platform_data->command_buffer
        renderCommandEncoderWithDescriptor:pass];
}

void Application::imgui_end_frame_platform() {
    ImGui_Impl_Mosaic_Metal_RenderDrawData(ImGui::GetDrawData(),
                                           m_platform_data->command_buffer,
                                           m_platform_data->encoder);

    [m_platform_data->encoder endEncoding];

    [m_platform_data->command_buffer presentDrawable:m_platform_data->drawable];
    [m_platform_data->command_buffer commit];
}
