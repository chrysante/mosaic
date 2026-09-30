#pragma once

#include "application/window.h"

#import <AppKit/NSWindow.h>

namespace mosaic {

enum class CocoaEventType : short { window_shall_zoom };

struct Window::Data { NSWindow* native_window = nil; };

} // namespace mosaic