#pragma once

#ifdef __OBJC__

@class NSEvent;
@class NSView;

namespace mosaic { class Window; }

bool ImGui_Impl_Mosaic_MacOS_Init(mosaic::Window& main_window);
void ImGui_Impl_Mosaic_MacOS_Shutdown();
void ImGui_Impl_Mosaic_MacOS_NewFrame(mosaic::Window& main_window);

#endif
