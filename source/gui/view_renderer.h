#pragma once

#include <imgui.h>
#include <utl/hashtable.hpp>

namespace mosaic {

class View;
class Window;
class ViewManager;
struct RuntimeViewState;

class ImGuiViewRenderer {
public:
    ImGuiViewRenderer();
    ~ImGuiViewRenderer();

    void begin_frame();

    void render_view(View& view);

    void end_frame();

private:
    RuntimeViewState& runtime_state(View const& view);
    RuntimeViewState& update_and_get_runtime_state(View const& view);

    utl::hashmap<View const*, RuntimeViewState> m_runtime;
    utl::hashset<ImGuiViewport const*> m_viewports_with_native_toolbar;
};

} // namespace mosaic