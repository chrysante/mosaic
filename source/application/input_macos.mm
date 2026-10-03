#include <AppKit/AppKit.h>
#include <exception>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/application.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_impl_macos.h"

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

static double2 get_location_in_window(NSEvent* event) {
    double2 p = event.locationInWindow;
    p.y = event.window.frame.size.height - p.y;
    return p;
}

static double2 get_mouse_location_on_screen() {
    double2 p = NSEvent.mouseLocation;
    p.y = CGDisplayPixelsHigh(kCGDirectMainDisplay) - p.y;
    return p;
}

static InputEventUnion make_mouse_button_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    vml::double2 location_in_window = get_location_in_window(event);
    vml::double2 location_on_screen = get_mouse_location_on_screen();
    switch (event.type) {
#define MOUSE_CASES(BUTTON)                                                    \
    case NSEventType##BUTTON##MouseDown:                                       \
        return BUTTON##MouseDownEvent(mod_flags, location_in_window,           \
                                      location_on_screen, event.clickCount);   \
    case NSEventType##BUTTON##MouseUp:                                         \
        return BUTTON##MouseUpEvent(mod_flags, location_in_window,             \
                                    location_on_screen);                       \
    case NSEventType##BUTTON##MouseDragged:                                    \
        return BUTTON##MouseDragEvent(mod_flags, location_in_window,           \
                                      location_on_screen,                      \
                                      {event.deltaX, event.deltaY});
        MOUSE_CASES(Left)
        MOUSE_CASES(Right)
        MOUSE_CASES(Other)
#undef MOUSE_CASES
    default:
        std::terminate();
    }
}

static InputEventUnion make_scroll_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    vml::double2 location_in_window = get_location_in_window(event);
    vml::double2 location_on_screen = get_mouse_location_on_screen();
    bool is_trackpad = event.momentumPhase != NSEventPhaseNone ||
                       event.phase != NSEventPhaseNone;
    return ScrollEvent(mod_flags, location_in_window, location_on_screen,
                       {event.deltaX, event.deltaY}, is_trackpad);
}

static InputEventUnion make_mouse_move_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    vml::double2 location_in_window = get_location_in_window(event);
    vml::double2 location_on_screen = get_mouse_location_on_screen();
    return MouseMoveEvent(mod_flags, location_in_window, location_on_screen,
                          {event.deltaX, event.deltaY});
}

static InputEventUnion make_magnification_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    vml::double2 location_in_window = get_location_in_window(event);
    vml::double2 location_on_screen = get_mouse_location_on_screen();
    return MagnificationEvent(mod_flags, location_in_window, location_on_screen,
                              event.magnification);
}

static InputEventUnion make_key_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    Key key = translate_key_code_german(event.keyCode);
    switch (event.type) {
    case NSEventTypeKeyDown:
        return KeyDownEvent(mod_flags, key, event.isARepeat ? 1 : 2);
    case NSEventTypeKeyUp:
        return KeyUpEvent(mod_flags, key);
    default:
        std::terminate();
    }
}

namespace {

using InputCallback = std::function<void(InputEventUnion const&)>;

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

    void invoke_all(InputEventUnion const& event) const {
        for (auto& [ID, callback] : map)
            callback(event);
    }

private:
    utl::hashmap<int, InputCallback> map;
};

CallbackMap g_callback_map;

} // namespace

static void create_global_monitors();

int mosaic::register_global_input_callback(
    std::function<void(InputEventUnion const&)> handler) {
    [[maybe_unused]] static int const _ = [] {
        create_global_monitors();
        return 0;
    }();
    return g_callback_map.add(std::move(handler));
}

void mosaic::unregister_global_input_callback(int ID) {
    g_callback_map.remove(ID);
}

static NSEvent* handle_event(NSEvent* event) {
    switch (event.type) {
    case NSEventTypeLeftMouseDown:
        [[fallthrough]];
    case NSEventTypeLeftMouseUp:
        [[fallthrough]];
    case NSEventTypeRightMouseDown:
        [[fallthrough]];
    case NSEventTypeLeftMouseDragged:
        [[fallthrough]];
    case NSEventTypeRightMouseUp:
        [[fallthrough]];
    case NSEventTypeRightMouseDragged:
        [[fallthrough]];
    case NSEventTypeOtherMouseDown:
        [[fallthrough]];
    case NSEventTypeOtherMouseUp:
        [[fallthrough]];
    case NSEventTypeOtherMouseDragged:
        g_callback_map.invoke_all(make_mouse_button_event(event));
        break;
    case NSEventTypeScrollWheel:
        g_callback_map.invoke_all(make_scroll_event(event));
        break;
    case NSEventTypeMouseMoved:
        g_callback_map.invoke_all(make_mouse_move_event(event));
        break;
    case NSEventTypeMagnify:
        g_callback_map.invoke_all(make_magnification_event(event));
        break;
    case NSEventTypeKeyUp:
        [[fallthrough]];
    case NSEventTypeKeyDown:
        g_callback_map.invoke_all(make_key_event(event));
        break;
    case NSEventTypeApplicationDefined:
        if ((internal::CocoaEventType)event.subtype ==
            internal::CocoaEventType::window_shall_zoom)
            [event.window zoom:nil];
        break;
    default:
        break;
    }
    return event;
}

static void create_global_monitors() {
    NSEventMask mask = 0;
    mask |= NSEventMaskMouseMoved;
    mask |= NSEventMaskFlagsChanged;
    mask |= NSEventMaskGesture;
    mask |= NSEventMaskMagnify;
    mask |= NSEventMaskSwipe;
    mask |= NSEventMaskRotate;
    mask |= NSEventMaskBeginGesture;
    mask |= NSEventMaskEndGesture;
    mask |= NSEventMaskSmartMagnify;
    mask |= NSEventMaskPressure;
    mask |= NSEventMaskDirectTouch;
    auto handler = ^(NSEvent* event) {
      return handle_event(event);
    };
    [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:handler];
}

void mosaic::register_window_for_input(Window& window) {
    internal::WindowImplMacOS::add_native_event_handler(
        window, [](NSEvent* event) { handle_event(event); });
}