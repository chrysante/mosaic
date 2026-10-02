#pragma once

#include "application/window.h"

#import <AppKit/NSWindow.h>

@interface WindowDelegateProxy : NSObject <NSWindowDelegate>
@property(nonatomic, assign) mosaic::Window* window;
@end

namespace mosaic {

enum class CocoaEventType : short { window_shall_zoom };

struct Window::Data {
    NSWindow* native_window = nil;
    WindowDelegateProxy* delegate = nil;
};

} // namespace mosaic