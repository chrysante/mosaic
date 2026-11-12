#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/hashtable.hpp>

#include "application/window_desc.h"
#include "gui/panel.h"

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
    RuntimePanelState& update_and_get_runtime_state(Panel& panel);

    utl::hashmap<Panel const*, RuntimePanelState> m_runtime;
    utl::hashmap<ImGuiViewport const*, WindowTitlebarSize>
        m_native_titlebar_size_map;
};

} // namespace mosaic