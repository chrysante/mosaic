#include "application/application.h"

#include <cstdlib>
#include <iostream>
#include <memory>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

#include "application/application_data.h"
#include "application/application_data_macos.h"
#include "application/window.h"
#include "application/window_data_macos.h"
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
    NSWindow* window = main_window()->m_data->native_window;
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

@interface MetalView : NSView
@end

@implementation MetalView

+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (void)layout {
    [super layout];
    [self updateDrawableSize];
}

- (void)updateDrawableSize {
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    CGFloat scale = self.window.backingScaleFactor;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(self.bounds.size.width * scale,
                                    self.bounds.size.height * scale);
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self updateDrawableSize];
}

@end

void Application::create_main_window_platform(Window* window) {
    NSWindow* native_window = window->m_data->native_window;
    MetalView* view =
        [[MetalView alloc] initWithFrame:native_window.contentView.bounds];
    CAMetalLayer* layer = [CAMetalLayer layer];

    layer.device = m_platform_data->device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    double scale = native_window.backingScaleFactor;
    layer.contentsScale = scale;
    // Prevent Core Animation from scaling the last drawable to fit the view
    // during live window resizing.
    layer.contentsGravity = kCAGravityTopLeft;
    layer.drawableSize = CGSizeMake(view.bounds.size.width * scale,
                                    view.bounds.size.height * scale);
    view.wantsLayer = YES;
    view.layer = layer;
    native_window.contentView = view;
    [view updateDrawableSize];
}

void Application::imgui_init_platform(Window* /* main_window */) {
    ImGui_Impl_Mosaic_MacOS_Init(*this);
    ImGui_Impl_Mosaic_Metal_Init(m_platform_data->device);
}

void Application::imgui_begin_frame_platform() {
    NSWindow* native_window = main_window()->m_data->native_window;
    NSView* view = native_window.contentView;
    CAMetalLayer* layer = (CAMetalLayer*)view.layer;

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
