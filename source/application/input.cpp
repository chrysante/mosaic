#include "application/input.h"

#include <ostream>

#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"

using namespace mosaic;
using namespace vml::short_types;

std::string mosaic::to_string(ModFlags flags) {
    std::string result = "[";
    bool any = false;
#define CASE(Name, Str)                                                        \
    if (test(flags & Name)) {                                                  \
        if (any) result += ", ";                                               \
        any = true;                                                            \
        result += Str;                                                         \
    }
    CASE(ModFlags::caps_lock, "caps_lock")
    CASE(ModFlags::shift, "shift")
    CASE(ModFlags::control, "control")
    CASE(ModFlags::alt, "alt")
    CASE(ModFlags::super, "super")
    CASE(ModFlags::num_lock, "num_lock")
#undef CASE
    result += "]";
    return result;
}

std::ostream& mosaic::operator<<(std::ostream& str, ModFlags flags) {
    return str << to_string(flags);
}

std::string MOSAIC_EXPORT mosaic::to_string(MouseButton button) {
    switch (button) {
    case MouseButton::left:
        return "left";
    case MouseButton::right:
        return "right";
    case MouseButton::other:
        return "other";
    }
}

std::ostream& MOSAIC_EXPORT mosaic::operator<<(std::ostream& str,
                                               MouseButton button) {
    return str << to_string(button);
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

    static void update_common(Input& input, InputEvent const& event) {
        input.m_mod_flags = event.modifier_flags;
    }

    static void update(Input& input, InputEvent const& event) {
        update_common(input, event);
        // clang-format off
        csp::visit(event, csp::overload {
            [&](MouseMoveEvent const& event) {
                update_mouse_pos(input, event.location_on_screen);
            },
            [&](MouseDragEvent const& event) {
                update_mouse_pos(input, event.location_on_screen);
            },
            [&](MouseDownEvent const& event) {
                input.m_mouse_button_down[(size_t)event.button] = true;
            },
            [&](MouseUpEvent const& event) {
                input.m_mouse_button_down[(size_t)event.button] = false;
            },
            [&](ScrollEvent const& event) {
                input.m_scroll_offset = event.offset;
            },
            [&](KeyDownEvent const& event) {
                input.m_key_down[(size_t)event.key] = true;
            },
            [&](KeyUpEvent const& event) {
                input.m_key_down[(size_t)event.key] = false;
            },
            [](InputEvent const&) {}
        });
        // clang-format on
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

void mosaic::input_end_frame(Input& input) { Impl::end_frame(input); }
