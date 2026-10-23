#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/hashtable.hpp>

namespace mosaic {

class Panel;
class Window;
class PanelManager;
struct RuntimePanelState;

class PanelRenderer {
public:
    PanelRenderer();
    ~PanelRenderer();

    void begin_frame();

    struct Result { bool focused; };

    Result render_panel(Panel& panel);

    void end_frame();

private:
    RuntimePanelState& runtime_state(Panel const& panel);
    RuntimePanelState& update_and_get_runtime_state(Panel const& panel);

    utl::hashmap<Panel const*, RuntimePanelState> m_runtime;
    utl::hashset<ImGuiViewport const*> m_viewports_with_native_toolbar;
};

} // namespace mosaic