#include "application/application_impl_macos.h"

#include <cstdlib>
#include <memory>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include "application/application.h"
#include "application/window.h"
#include "application/window_impl_macos.h"
#include "imgui/imgui_impl_mosaic_macos.h"
#include "imgui/imgui_impl_mosaic_metal.h"
#include "support/logger.h"

using namespace mosaic;

using internal::ApplicationImpl;
using internal::ApplicationImplMacOS;

std::unique_ptr<ApplicationImpl>
ApplicationImpl::create_impl(Application& app) {
    return std::make_unique<ApplicationImplMacOS>(app);
}

void Application::quit() { [APP_IMPL.native_app stop:nil]; }

void ApplicationImplMacOS::init_platform() {
    @autoreleasepool {
        native_app = [NSApplication sharedApplication];
        [native_app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [native_app activateIgnoringOtherApps:YES];
        device = MTLCreateSystemDefaultDevice();
        if (!device) {
            // Metal isn't available
            Logger::error("Metal is not available\n");
            std::exit(EXIT_FAILURE);
        }
        command_queue = [device newCommandQueue];
    }
}

@implementation DisplayLinkTarget
- (void)displayLinkCallback:(CADisplayLink*)displayLink {
    _app_impl->run_one_frame();
}
@end

int ApplicationImplMacOS::run_platform() {
    NSWindow* window = GET_WINDOW_IMPL(*app.main_window()).native_window;
    NSView* view = window.contentView;
    display_link_target = [[DisplayLinkTarget alloc] init];
    display_link_target.app_impl = this;
    display_link = [view displayLinkWithTarget:display_link_target
                                      selector:@selector(displayLinkCallback:)];
    [display_link addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
    [native_app run];
    return 0;
}

void ApplicationImplMacOS::shutdown_platform() {
    ImGui_Impl_Mosaic_MacOS_Shutdown();
}

void ApplicationImplMacOS::create_main_window_platform(Window& window) {
    metal_layer = WindowImplMacOS::create_metal_layer(device, window);
}

void ApplicationImplMacOS::imgui_init_platform() {
    ImGui_Impl_Mosaic_MacOS_Init(app);
    ImGui_Impl_Mosaic_Metal_Init(device);
}

void ApplicationImplMacOS::imgui_begin_frame_platform() {
    drawable = [metal_layer nextDrawable];
    if (!drawable) {
        Logger::error("Have no drawable\n");
        return;
    }
    MTLRenderPassDescriptor* pass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
    command_buffer = [command_queue commandBuffer];
    ImGui_Impl_Mosaic_Metal_NewFrame(pass);
    ImGui_Impl_Mosaic_MacOS_NewFrame(*app.main_window());
    encoder = [command_buffer renderCommandEncoderWithDescriptor:pass];
}

void ApplicationImplMacOS::imgui_end_frame_platform() {
    ImGui_Impl_Mosaic_Metal_RenderDrawData(ImGui::GetDrawData(), command_buffer,
                                           encoder);
    [encoder endEncoding];
    [command_buffer presentDrawable:drawable];
    [command_buffer commit];
}
