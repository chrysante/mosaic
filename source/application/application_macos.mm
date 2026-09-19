#include "application/application.h"
#include "application/application_data.h"

#include <AppKit/AppKit.h>
#include <memory>

using namespace mosaic;

struct Application::PlatformData {
    NSApplication* app;
};

Application::PlatformData* Application::create_platform_data() {
    return new PlatformData{};
}

void Application::destroy_platform_data(PlatformData* data) {
    delete data;
}

void Application::platform_poll_events() {
    auto* app = m_platform_data->app;
    @autoreleasepool {
        NSEvent* event;
        NSDate* until = m_data->run_loop_mode == RunLoopMode::Wait ? [NSDate distantFuture]
                                                                   : [NSDate distantPast];

        while ((event = [app nextEventMatchingMask:NSEventMaskAny
                                         untilDate:until
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES])) {
            [app sendEvent:event];
        }
    }
}

void Application::platform_init() {
    @autoreleasepool {
        NSApplication* app = m_platform_data->app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app activateIgnoringOtherApps:YES];
    }
}
