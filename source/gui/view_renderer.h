#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/hashtable.hpp>
#include <vml/vml.hpp>

#include "application/window.h"

namespace mosaic {

class View;
class Window;
class ViewManager;
struct RuntimeViewState;

class ImGuiViewRenderer {
public:
    explicit ImGuiViewRenderer(ViewManager& manager);
    ~ImGuiViewRenderer();

    void begin_frame();

    void render_view(View& view);

    /// Must be called after all views have been submitted for the frame.
    void update_platform_window_styles();

private:
    struct PlatformWindowState {
        Window* window = nullptr;
        bool should_have_toolbar_style = false;
    };

    RuntimeViewState& runtime_state(View const& view);

    void begin_window(View& view);

    RuntimeViewState& update_and_get_runtime_state(View const& view);

    // void update_native_window_style(View& view);

    void display_toolbar(View& view, RuntimeViewState& runtime);

    void update_runtime_state(View& view, ImGuiWindow* window,
                              ImGuiViewport* viewport);

    bool is_main_viewport(ImGuiViewport const* viewport) const;

    // bool is_single_view_in_platform_window(View const& view,
    //                                        ImGuiViewport const* viewport)
    //                                        const;

    std::string make_imgui_id(View const& view) const;

    ViewManager& m_manager;

    utl::hashmap<View const*, RuntimeViewState> m_runtime;
    utl::hashset<ImGuiViewport const*> m_viewports_with_native_toolbar;
};

} // namespace mosaic