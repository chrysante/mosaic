#pragma once

struct ImGuiViewport;

namespace mosaic {

class Application;
class Window;
struct WindowStyle;

WindowStyle make_window_style(ImGuiViewport const* viewport);

bool imgui_impl_init(Application& application);
void imgui_impl_shutdown();
void imgui_impl_new_frame(Window& main_window);

} // namespace mosaic
