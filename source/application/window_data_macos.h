#pragma once

#include "application/window.h"

#import <AppKit/NSWindow.h>

namespace mosaic {

struct Window::Data {
    NSWindow* native_window = nil;
};

} // namespace mosaic