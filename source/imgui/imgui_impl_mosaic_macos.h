#pragma once

#include "application/application.h"
#ifdef __OBJC__

@class NSEvent;
@class NSView;

namespace mosaic {

class Application;
class Window;

} // namespace mosaic

bool ImGui_Impl_Mosaic_MacOS_Init(mosaic::Application& application);
void ImGui_Impl_Mosaic_MacOS_Shutdown();
void ImGui_Impl_Mosaic_MacOS_NewFrame(mosaic::Window& main_window);

#endif
