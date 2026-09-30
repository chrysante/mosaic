#include <AppKit/AppKit.h>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/application.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_data_macos.h"

using namespace mosaic;
using namespace vml::short_types;

static Key translate_key_code_german(unsigned code) {
    using enum Key;
    switch (code) {
    case 53:
        return Key::escape;
    case 122:
        return Key::F1;
    case 120:
        return Key::F2;
    case 99:
        return Key::F3;
    case 118:
        return Key::F4;
    case 96:
        return Key::F5;
    case 98:
        return Key::F7;
    case 100:
        return Key::F8;
    case 101:
        return Key::F9;
    case 109:
        return Key::F10;
    case 103:
        return Key::F11;
    case 111:
        return Key::F12;
    case 10:
        return Key::circumflex;
    case 18:
        return Key::num1;
    case 19:
        return Key::num2;
    case 20:
        return Key::num3;
    case 21:
        return Key::num4;
    case 23:
        return Key::num5;
    case 22:
        return Key::num6;
    case 26:
        return Key::num7;
    case 28:
        return Key::num8;
    case 25:
        return Key::num9;
    case 29:
        return Key::num0;
    case 24:
        return Key::accent;
    case 51:
        return Key::backspace;
    case 48:
        return Key::tab;
    case 12:
        return Key::Q;
    case 13:
        return Key::W;
    case 14:
        return Key::E;
    case 15:
        return Key::R;
    case 17:
        return Key::T;
    case 16:
        return Key::Z;
    case 32:
        return Key::U;
    case 34:
        return Key::I;
    case 31:
        return Key::O;
    case 35:
        return Key::P;
    case 30:
        return Key::plus;
    case 36:
        return Key::enter;
    case 0:
        return Key::A;
    case 1:
        return Key::S;
    case 2:
        return Key::D;
    case 3:
        return Key::F;
    case 5:
        return Key::G;
    case 4:
        return Key::H;
    case 38:
        return Key::J;
    case 40:
        return Key::K;
    case 37:
        return Key::L;
    case 42:
        return Key::hash;
    case 50:
        return Key::angle_bracket_open;
    case 6:
        return Key::Y;
    case 7:
        return Key::X;
    case 8:
        return Key::C;
    case 9:
        return Key::V;
    case 11:
        return Key::B;
    case 45:
        return Key::N;
    case 46:
        return Key::M;
    case 43:
        return Key::comma;
    case 47:
        return Key::period;
    case 44:
        return Key::minus;
    case 49:
        return Key::space;
    case 126:
        return Key::up_arrow;
    case 125:
        return Key::down_arrow;
    case 123:
        return Key::left_arrow;
    case 124:
        return Key::right_arrow;
    default:
        return Key::none;
    }
}

static MouseButton translate_mouse_button(NSEventType type) {
    switch (type) {
    case NSEventTypeLeftMouseDown:
        [[fallthrough]];
    case NSEventTypeLeftMouseUp:
        return MouseButton::left;
    case NSEventTypeRightMouseDown:
        [[fallthrough]];
    case NSEventTypeRightMouseUp:
        return MouseButton::right;
    case NSEventTypeOtherMouseDown:
        [[fallthrough]];
    case NSEventTypeOtherMouseUp:
        return MouseButton::other;
    default:
        return MouseButton::none;
    }
}

static bool is_mouse_button_down(NSEventType type) {
    switch (type) {
    case NSEventTypeLeftMouseUp:
        [[fallthrough]];
    case NSEventTypeRightMouseUp:
        [[fallthrough]];
    case NSEventTypeOtherMouseUp: {
        return false;
    }
    case NSEventTypeLeftMouseDown:
        [[fallthrough]];
    case NSEventTypeRightMouseDown:
        [[fallthrough]];
    case NSEventTypeOtherMouseDown: {
        return true;
    }
    default:
        assert(false);
    }
}

static ModFlags translate_mod_flags(NSEventModifierFlags flags) {
    using enum ModFlags;
    ModFlags result = none;
    if (flags & NSEventModifierFlagCapsLock) {
        result |= caps_lock;
    }
    if (flags & NSEventModifierFlagShift) {
        result |= shift;
    }
    if (flags & NSEventModifierFlagControl) {
        result |= control;
    }
    if (flags & NSEventModifierFlagOption) {
        result |= alt;
    }
    if (flags & NSEventModifierFlagCommand) {
        result |= super;
    }
    if (flags & NSEventModifierFlagNumericPad) {
        result |= num_lock;
    }
    return result;
}

static InputEventMask translate_type(NSEventType type) {
    switch (type) {
    case NSEventTypeLeftMouseDown:
        return InputEventMask::left_mouse_down;
    case NSEventTypeLeftMouseUp:
        return InputEventMask::left_mouse_up;
    case NSEventTypeRightMouseDown:
        return InputEventMask::right_mouse_down;
    case NSEventTypeRightMouseUp:
        return InputEventMask::right_mouse_up;
    case NSEventTypeOtherMouseDown:
        return InputEventMask::other_mouse_down;
    case NSEventTypeOtherMouseUp:
        return InputEventMask::other_mouse_up;
    case NSEventTypeKeyDown:
        return InputEventMask::key_down;
    case NSEventTypeKeyUp:
        return InputEventMask::key_up;
    default:
        return InputEventMask::none;
    }
}

static double2 get_location_in_window(NSEvent* event) {
    double2 p = event.locationInWindow;
    p.y = event.window.frame.size.height - p.y;
    return p;
}

static double2 get_mouseL_location_on_screen() {
    double2 p = NSEvent.mouseLocation;
    p.y = CGDisplayPixelsHigh(kCGDirectMainDisplay) - p.y;
    return p;
}

static MouseEvent make_mouse_event(NSEvent* event) {
    MouseEvent result{};
    result.modifier_flags = translate_mod_flags(event.modifierFlags);
    result.location_in_window = get_location_in_window(event);
    result.location_on_screen = get_mouseL_location_on_screen();
    return result;
}

static InputEvent make_mouse_button_event(NSEvent* event) {
    MouseEvent base = make_mouse_event(event);
    auto type = translate_type(event.type);
    MouseButton button = translate_mouse_button(event.type);
    if (is_mouse_button_down(event.type)) {
        MouseDownEvent downEvent{base, button, event.clickCount};
        return InputEvent(type, downEvent);
    } else {
        MouseUpEvent upEvent{base, button};
        return InputEvent(type, upEvent);
    }
}

static InputEvent make_scroll_event(NSEvent* event) {
    ScrollEvent result{make_mouse_event(event),
                       {event.scrollingDeltaX, event.scrollingDeltaY},
                       event.momentumPhase != NSEventPhaseNone ||
                           event.phase != NSEventPhaseNone};
    return InputEvent(InputEventMask::scroll_wheel, result);
}

static InputEvent make_mouse_move_drag_event(NSEvent* event) {
    MouseMoveEvent base{make_mouse_event(event), {event.deltaX, event.deltaY}};
    switch (event.type) {
    case NSEventTypeMouseMoved:
        return InputEvent(InputEventMask::mouse_moved, base);
    case NSEventTypeLeftMouseDragged:
        return InputEvent(InputEventMask::left_mouse_dragged,
                          MouseDragEvent{base, MouseButton::left});
    case NSEventTypeRightMouseDragged:
        return InputEvent(InputEventMask::right_mouse_dragged,
                          MouseDragEvent{base, MouseButton::right});
    case NSEventTypeOtherMouseDragged:
        return InputEvent(InputEventMask::other_mouse_dragged,
                          MouseDragEvent{base, MouseButton::other});
    default:
        assert(false); // Invalid cocoa event
    }
}

static InputEvent make_magnification_event(NSEvent* event) {
    MagnificationEvent result{make_mouse_event(event), event.magnification};
    return InputEvent(InputEventMask::magnify, result);
}

static InputEvent make_key_event(NSEvent* event) {
    KeyEvent keyEvent{
        {.modifier_flags = translate_mod_flags(event.modifierFlags)},
        translate_key_code_german(event.keyCode),
        1};
    return InputEvent(translate_type(event.type), keyEvent);
}

namespace {

using InputCallback = std::function<void(InputEvent const&, void*)>;

class CallbackMap {
public:
    int add(InputCallback callback) {
        static int ID = 0;
        bool result = map.insert({ID, std::move(callback)}).second;
        assert(result && "Failed to register callback");
        return ID++;
    }

    void remove(int ID) {
        bool erased = map.erase(ID);
        assert(
            erased &&
            "Failed to unregister callback. Has this callback been unregistered before?");
    }

    void invoke_all(InputEvent const& event, NSEvent* native) const {
        for (auto& [ID, callback] : map)
            callback(event, (__bridge void*)native);
    }

private:
    utl::hashmap<int, InputCallback> map;
};

CallbackMap g_callback_map;

} // namespace

static void create_global_monitors();

int mosaic::register_global_input_callback(
    std::function<void(InputEvent const&, void*)> handler) {
    [[maybe_unused]] static int const _ = [] {
        create_global_monitors();
        return 0;
    }();
    return g_callback_map.add(std::move(handler));
}

void mosaic::unregister_global_input_callback(int ID) {
    g_callback_map.remove(ID);
}

/// \Returns `true` if the location of \p event is over the frame of \p button
static bool is_over_window_button(NSEvent* event, NSWindowButton button) {
    auto button_frame = [event.window standardWindowButton:button].frame;
    auto window_frame = event.window.frame;
    button_frame.origin.y = window_frame.size.height - button_frame.origin.y -
                            button_frame.size.height;
    return NSPointInRect(event.locationInWindow, button_frame);
}

/// \Returns `true` if \p value is in the interval `[min + padding, max -
/// padding]`
static bool is_in_bounds(double value, double min, double max, double padding) {
    return value >= min + padding && value <= max - padding;
}

/// \Returns `true` if the location of \p event is within the area of the window
/// where the window is resizeable, i.e. in any window location where the mouse
/// image changes to a resize indicator. This is used to avoid starting window
/// drag operations when resizing
static bool is_in_resize_area(NSEvent* event) {
    // Padding     Corner Padding
    // vvv         vvvvv
    // +---------------+ -<
    // |   +-------+   | |< Corner Padding
    // |   |       |   | |<
    // | +-+       +-+ | -<
    // | |           | |
    // | |           | |
    // | +-+       +-+ |
    // |   |       |   |
    // |   +-------+   | -< Padding
    // +---------------+ -<
    //
    // The set difference of the shapes above is the resize area. The padding
    // values used below are determined by experimentation
    auto frame = event.window.frame;
    auto location = event.locationInWindow;
    // Borders
    double padding = 3;
    if (!is_in_bounds(location.x, 0, frame.size.width, padding) ||
        !is_in_bounds(location.y, 0, frame.size.height, padding)) {
        return true;
    }
    // Corners
    double corner_padding = 12;
    if (!is_in_bounds(location.x, 0, frame.size.width, corner_padding) &&
        !is_in_bounds(location.y, 0, frame.size.height, corner_padding)) {
        return true;
    }
    return false;
}

/// \Returns `true` if \p event may be used to start a window drag operation
/// without interfering with the window buttons and resize operations
static bool is_resize_or_window_button_interaction(NSEvent* event) {
    return is_in_resize_area(event) ||
           is_over_window_button(event, NSWindowCloseButton) ||
           is_over_window_button(event, NSWindowMiniaturizeButton) ||
           is_over_window_button(event, NSWindowZoomButton);
}

static NSEvent* handle_event(NSEvent* event) {
    switch (event.type) {
    case NSEventTypeLeftMouseDown:
        [[fallthrough]];
    case NSEventTypeLeftMouseUp:
        [[fallthrough]];
    case NSEventTypeRightMouseDown:
        [[fallthrough]];
    case NSEventTypeRightMouseUp:
        [[fallthrough]];
    case NSEventTypeOtherMouseDown:
        [[fallthrough]];
    case NSEventTypeOtherMouseUp:
        /// Any events that interact with
        /// window resizing or the window
        /// buttons are not forwarded to
        /// the application
        if (!is_resize_or_window_button_interaction(event) ||
            !is_mouse_button_down(event.type)) {
            g_callback_map.invoke_all(make_mouse_button_event(event), event);
        }
        break;
    case NSEventTypeScrollWheel:
        g_callback_map.invoke_all(make_scroll_event(event), event);
        break;
    case NSEventTypeMouseMoved:
        [[fallthrough]];
    case NSEventTypeLeftMouseDragged:
        [[fallthrough]];
    case NSEventTypeRightMouseDragged:
        [[fallthrough]];
    case NSEventTypeOtherMouseDragged:
        g_callback_map.invoke_all(make_mouse_move_drag_event(event), event);
        break;
    case NSEventTypeMagnify:
        g_callback_map.invoke_all(make_magnification_event(event), event);
        break;
    case NSEventTypeKeyUp:
        [[fallthrough]];
    case NSEventTypeKeyDown:
        g_callback_map.invoke_all(make_key_event(event), event);
        break;
    case NSEventTypeApplicationDefined:
        if ((CocoaEventType)event.subtype == CocoaEventType::window_shall_zoom)
            [event.window zoom:nil];
        break;
    case NSEventTypeFlagsChanged: {
        // Just notify our app that something happened
        NSEvent* event =
            [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                               location:NSMakePoint(0, 0)
                          modifierFlags:0
                              timestamp:0
                           windowNumber:0
                                context:nil
                                subtype:0
                                  data1:0
                                  data2:0];
        [NSApp postEvent:event atStart:YES];
        break;
    }
    default:
        break;
    }
    return event;
}

static void create_global_monitors() {
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                          handler:^(NSEvent* event) {
                                            return handle_event(event);
                                          }];
}
