#include "application/input.h"

#include <ostream>

#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"

using namespace mosaic;
using namespace vml::short_types;

std::ostream& mosaic::operator<<(std::ostream& str, ModFlags flags) {
    using enum ModFlags;
    str << "[";
    bool any = false;
#define CASE(Name)                                                             \
    if (test(flags & Name)) {                                                  \
        if (any) {                                                             \
            str << ", ";                                                       \
        }                                                                      \
        any = true;                                                            \
        str << #Name;                                                          \
    }
    CASE(caps_lock)
    CASE(shift)
    CASE(control)
    CASE(alt)
    CASE(super)
    CASE(num_lock)
#undef CASE
    return str << "]";
}

vml::float2 Input::mouse_position_in_window(Window const& window) const {
    return mouse_position() - window.position();
}

struct Input::Impl {
    static void update_mouse_pos(Input& input, double2 position) {
        auto last_pos = input.m_mouse_position;
        input.m_mouse_position = position;
        input.m_mouse_offset = position - last_pos;
    }

    static void update_common(Input& input, auto const& event) {
        if constexpr (requires { event.mod_flags; })
            input.m_mod_flags = event.mod_flags;
    }

    static void update(Input& input, InputEvent const& event) {
        using enum InputEventMask;
        event.handle<mouse_moved>([&](MouseEvent const& event) {
            update_common(input, event);
            update_mouse_pos(input, event.location_on_screen);
        });
        event.handle<mouse_dragged>([&](MouseEvent const& event) {
            update_common(input, event);
            update_mouse_pos(input, event.location_on_screen);
        });
        event.handle<mouse_down>([&](MouseDownEvent const& event) {
            update_common(input, event);
            input.m_mouse_button_down[(size_t)event.button] = true;
        });
        event.handle<mouse_up>([&](MouseUpEvent const& event) {
            update_common(input, event);
            input.m_mouse_button_down[(size_t)event.button] = false;
        });
        event.handle<scroll_wheel>([&](ScrollEvent const& event) {
            update_common(input, event);
            input.m_scroll_offset = event.offset;
        });
        event.handle<key_down>([&](KeyEvent const& event) {
            update_common(input, event);
            input.m_key_down[(size_t)event.key] = true;
        });
        event.handle<key_up>([&](KeyEvent const& event) {
            update_common(input, event);
            input.m_key_down[(size_t)event.key] = false;
        });
    }

    static void end_frame(Input& input) {
        input.m_mouse_offset = {0.0, 0.0};
        input.m_scroll_offset = {0.0, 0.0};
    }
};

using Impl = Input::Impl;

void mosaic::input_update(Input& input, InputEvent const& event) {
    Impl::update(input, event);
}

void mosaic::input_end_frame(Input& input) {
    Impl::end_frame(input);
}
