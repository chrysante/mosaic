#pragma once

#include <vml/vml.hpp>

namespace mosaic {

struct WindowStyle {
    /// If set to `true` the window will be created without a titlebar and
    /// without window buttons
    bool no_decoration = false;

    /// If set to `true` the title bar will be larger
    bool has_toolbar_area = false;
};

/// Constructor arguments for a window
struct WindowDesc {
    /// The initial position of the window
    vml::int2 position;

    /// The initial size of the window
    vml::int2 size;

    /// If set to `true` the window will be created focused
    bool focused = true;

    ///
    WindowStyle style;
};

} // namespace mosaic