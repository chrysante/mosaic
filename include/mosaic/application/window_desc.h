#pragma once

#include <string>

#include <utl/common.hpp>
#include <vml/vml.hpp>

namespace mosaic {

enum class WindowDecoration {
    normal,
    none,
};

enum class WindowCapability : unsigned {
    none = 0,
    resizable = 1 << 0,
    closable = 1 << 1,
    minimizable = 1 << 2,
    zoomable = 1 << 3,
};

UTL_ENUM_OPERATORS(WindowCapability)

struct WindowStyle {
    WindowDecoration decoration = WindowDecoration::normal;

    WindowCapability capabilities =
        WindowCapability::resizable | WindowCapability::closable |
        WindowCapability::minimizable | WindowCapability::zoomable;

    /// Allows the content view to extend into the title bar.
    bool full_size_content = false;

    /// Use the larger/unified title-bar appearance.
    bool large_titlebar = false;

    bool operator==(WindowStyle const&) const = default;
};

struct WindowDesc {
    /// Initial position of the window.
    vml::int2 position;

    /// Initial size of the window.
    vml::int2 size;

    /// Size constraints
    // FIXME: Not yet honored
    vml::int2 min_size = {0, 0}, max_size = {INT_MAX, INT_MAX};

    /// Initial window title.
    std::string title;

    /// Whether the window is initially visible.
    bool visible = true;

    /// Whether the window should initially become key/focused.
    bool focused = true;

    /// Whether the window should stay above normal windows.
    bool topmost = false;

    WindowStyle style;
};

} // namespace mosaic