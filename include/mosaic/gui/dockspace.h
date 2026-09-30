#pragma once

#include <vml/vml.hpp>

#include <mosaic/application/window.h>
#include <mosaic/gui/toolbar.h>

namespace mosaic {

class Window;

/// Wrapper around ImGui's docking feature
class Dockspace {
public:
    /// Makes the entire content area of \p window a dockable area and draws
    /// toolbars in the windows titlebar
    void display(Window* window);

    /// # Toolbar Layout
    /// A dockspace has three toolbars. Depending on the docked windows in the
    /// dockspace a layout is for the three toolbars is automatically chosen.
    /// For example if there is a window on the left and on the right side of
    /// the dock space which are not larger than one third of the dockspace, the
    /// toolbars are aligned to these windows:
    /// ```
    /// ┌──────────────┬──────────────────┬───────────────┐
    /// │ Left Toolbar │ Center Toolbar   │ Right Toolbar │
    /// ├──────────────┼──────────────────┼───────────────┤
    /// │              │                  │               │
    /// │ Left Window  │ Center Window(s) │ Right Window  │
    /// │              │                  │               │
    /// └──────────────┴──────────────────┴───────────────┘
    /// ┌──────────────┬──────────────────┬───────────────┐
    /// │ Left Toolbar │ Center Toolbar   │ Right Toolbar │
    /// ├──────────────┼──────────────────┴───────────────┤
    /// │              │                                  │
    /// │ Left Window  │        Other Windows             │
    /// │              │                                  │
    /// └──────────────┴──────────────────────────────────┘
    /// ┌──────────────┬──────────────────┬───────────────┐
    /// │ Left Toolbar │ Center Toolbar   │ Right Toolbar │
    /// ├──────────────┴──────────────────┼───────────────┤
    /// │                                 │               │
    /// │          Left Windows           │ Right Window  │
    /// │                                 │               │
    /// └─────────────────────────────────┴───────────────┘
    /// ┌──────────────┬──────────────────┬───────────────┐
    /// │ Left Toolbar │ Center Toolbar   │ Right Toolbar │
    /// ├──────────────┴──────────────────┴───────────────┤
    /// │                                                 │
    /// │ Single window or windows that don't meet the    │
    /// │ requirements                                    │
    /// │                                                 │
    /// └─────────────────────────────────────────────────┘
    /// ```

    /// Assigns the left toolbar
    void set_left_toolbar(Toolbar toolbar);

    /// Assigns the center toolbar
    void set_center_toolbar(Toolbar toolbar);

    /// Assigns the right toolbar
    void set_right_toolbar(Toolbar toolbar);

    /// Focuses the last focused child window in the dockspace
    void take_focus();

private:
    void main_window();

    void display_toolbars(Window& window);

    vml::float2 min_window_size = {250, 100};
    std::array<Toolbar, 3> toolbars;

    void* main_imgui_window = nullptr;
    size_t spacing_count = 0;
    unsigned main_dock_id = 0;
};

} // namespace mosaic
