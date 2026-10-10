#include "application/input.h"

#include <Foundation/NSObjCRuntime.h>
#include <exception>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>
#include <vml/vml.hpp>

#include "application/application.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_impl_macos.h"

using namespace mosaic;
using namespace vml::short_types;

namespace {
struct KeyTranslationResult {
    Key logical;
    PhysicalKey physical;
};
} // namespace

static KeyTranslationResult translate_key_code(NSEvent* event) {
    // =========================================================================
    // Physical key
    // =========================================================================
    //
    // Logical key is set here if possible but might be overridden later

    KeyTranslationResult result = [&] {
        switch (event.keyCode) {
#define CASE(native, physical, logical)                                        \
    case native:                                                               \
        return KeyTranslationResult{Key::logical, PhysicalKey::physical};
            // clang-format off
            
    // Letters
    CASE(kVK_ANSI_A, A, A)
    CASE(kVK_ANSI_S, S, S)
    CASE(kVK_ANSI_D, D, D)
    CASE(kVK_ANSI_F, F, F)
    CASE(kVK_ANSI_H, H, H)
    CASE(kVK_ANSI_G, G, G)
    CASE(kVK_ANSI_Z, Z, Z)
    CASE(kVK_ANSI_X, X, X)
    CASE(kVK_ANSI_C, C, C)
    CASE(kVK_ANSI_V, V, V)
    CASE(kVK_ANSI_B, B, B)
    CASE(kVK_ANSI_Q, Q, Q)
    CASE(kVK_ANSI_W, W, W)
    CASE(kVK_ANSI_E, E, E)
    CASE(kVK_ANSI_R, R, R)
    CASE(kVK_ANSI_Y, Y, Y)
    CASE(kVK_ANSI_T, T, T)
    CASE(kVK_ANSI_O, O, O)
    CASE(kVK_ANSI_U, U, U)
    CASE(kVK_ANSI_I, I, I)
    CASE(kVK_ANSI_P, P, P)
    CASE(kVK_ANSI_L, L, L)
    CASE(kVK_ANSI_J, J, J)
    CASE(kVK_ANSI_K, K, K)
    CASE(kVK_ANSI_N, N, N)
    CASE(kVK_ANSI_M, M, M)

    // Number row
    CASE(kVK_ANSI_1, num1, _1)
    CASE(kVK_ANSI_2, num2, _2)
    CASE(kVK_ANSI_3, num3, _3)
    CASE(kVK_ANSI_4, num4, _4)
    CASE(kVK_ANSI_5, num5, _5)
    CASE(kVK_ANSI_6, num6, _6)
    CASE(kVK_ANSI_7, num7, _7)
    CASE(kVK_ANSI_8, num8, _8)
    CASE(kVK_ANSI_9, num9, _9)
    CASE(kVK_ANSI_0, num0, _0)

    // Symbols
    CASE(kVK_ANSI_Equal,        equal,         equal)
    CASE(kVK_ANSI_Minus,        minus,         minus)
    CASE(kVK_ANSI_RightBracket, right_bracket, right_bracket)
    CASE(kVK_ANSI_LeftBracket,  left_bracket,  left_bracket)
    CASE(kVK_ANSI_Quote,        apostrophe,    apostrophe)
    CASE(kVK_ANSI_Semicolon,    semicolon,     semicolon)
    CASE(kVK_ANSI_Backslash,    backslash,     backslash)
    CASE(kVK_ANSI_Comma,        comma,         comma)
    CASE(kVK_ANSI_Slash,        slash,         slash)
    CASE(kVK_ANSI_Period,       period,        period)
    CASE(kVK_ANSI_Grave,        grave_accent,  grave_accent)

    // Non-character keys
    CASE(kVK_Return,        enter,       enter)
    CASE(kVK_Tab,           tab,         tab)
    CASE(kVK_Space,         space,       space)
    CASE(kVK_Delete,        backspace,   backspace)
    CASE(kVK_Escape,        escape,      escape)
    CASE(kVK_CapsLock,      caps_lock,   caps_lock)
    CASE(kVK_ForwardDelete, Delete,      Delete)
    CASE(kVK_Home,          home,        home)
    CASE(kVK_End,           end,         end)
    CASE(kVK_PageUp,        page_up,     page_up)
    CASE(kVK_PageDown,      page_down,   page_down)
    CASE(kVK_LeftArrow,     left_arrow,  left_arrow)
    CASE(kVK_RightArrow,    right_arrow, right_arrow)
    CASE(kVK_DownArrow,     down_arrow,  down_arrow)
    CASE(kVK_UpArrow,       up_arrow,    up_arrow)

    // Function keys
    CASE(kVK_F1,  F1,  F1)
    CASE(kVK_F2,  F2,  F2)
    CASE(kVK_F3,  F3,  F3)
    CASE(kVK_F4,  F4,  F4)
    CASE(kVK_F5,  F5,  F5)
    CASE(kVK_F6,  F6,  F6)
    CASE(kVK_F7,  F7,  F7)
    CASE(kVK_F8,  F8,  F8)
    CASE(kVK_F9,  F9,  F9)
    CASE(kVK_F10, F10, F10)
    CASE(kVK_F11, F11, F11)
    CASE(kVK_F12, F12, F12)

    // Keypad
    CASE(kVK_ANSI_Keypad0,        keypad0, _0)
    CASE(kVK_ANSI_Keypad1,        keypad1, _1)
    CASE(kVK_ANSI_Keypad2,        keypad2, _2)
    CASE(kVK_ANSI_Keypad3,        keypad3, _3)
    CASE(kVK_ANSI_Keypad4,        keypad4, _4)
    CASE(kVK_ANSI_Keypad5,        keypad5, _5)
    CASE(kVK_ANSI_Keypad6,        keypad6, _6)
    CASE(kVK_ANSI_Keypad7,        keypad7, _7)
    CASE(kVK_ANSI_Keypad8,        keypad8, _8)
    CASE(kVK_ANSI_Keypad9,        keypad9, _9)
    CASE(kVK_ANSI_KeypadEnter,    keypad_enter,    enter)
    CASE(kVK_ANSI_KeypadDecimal,  keypad_decimal,  keypad_decimal)
    CASE(kVK_ANSI_KeypadMultiply, keypad_multiply, keypad_multiply)
    CASE(kVK_ANSI_KeypadPlus,     keypad_add,      keypad_add)
    CASE(kVK_ANSI_KeypadDivide,   keypad_divide,   keypad_divide)
    CASE(kVK_ANSI_KeypadMinus,    keypad_subtract, keypad_subtract)
    CASE(kVK_ANSI_KeypadEquals,   keypad_equal,    keypad_equal)
    CASE(kVK_ANSI_KeypadClear,    num_lock,        num_lock)
#undef CASE
            // clang-format on
        default:
            return KeyTranslationResult{Key::unknown, PhysicalKey::unknown};
        }
    }();

    // =========================================================================
    // Physical key
    // =========================================================================

    // Character-producing keys are resolved according to the current
    // keyboard layout.
    NSString* characters = event.charactersIgnoringModifiers;
    if (characters.length == 1) {
        unichar c = [characters characterAtIndex:0];
        auto ignore = [](PhysicalKey physical, auto... args) {
            return (... || (physical == args));
        };
        switch (c) {
#define CASE(character, key, ...)                                              \
    case character:                                                            \
        if (!ignore(result.physical __VA_OPT__(, ) __VA_ARGS__))               \
            result.logical = Key::key;                                         \
        break;
            CASE('a', A)
            CASE('b', B)
            CASE('c', C)
            CASE('d', D)
            CASE('e', E)
            CASE('f', F)
            CASE('g', G)
            CASE('h', H)
            CASE('i', I)
            CASE('j', J)
            CASE('k', K)
            CASE('l', L)
            CASE('m', M)
            CASE('n', N)
            CASE('o', O)
            CASE('p', P)
            CASE('q', Q)
            CASE('r', R)
            CASE('s', S)
            CASE('t', T)
            CASE('u', U)
            CASE('v', V)
            CASE('w', W)
            CASE('x', X)
            CASE('y', Y)
            CASE('z', Z)
            CASE('A', A)
            CASE('B', B)
            CASE('C', C)
            CASE('D', D)
            CASE('E', E)
            CASE('F', F)
            CASE('G', G)
            CASE('H', H)
            CASE('I', I)
            CASE('J', J)
            CASE('K', K)
            CASE('L', L)
            CASE('M', M)
            CASE('N', N)
            CASE('O', O)
            CASE('P', P)
            CASE('Q', Q)
            CASE('R', R)
            CASE('S', S)
            CASE('T', T)
            CASE('U', U)
            CASE('V', V)
            CASE('W', W)
            CASE('X', X)
            CASE('Y', Y)
            CASE('Z', Z)
            CASE('0', _0)
            CASE('1', _1)
            CASE('2', _2)
            CASE('3', _3)
            CASE('4', _4)
            CASE('5', _5)
            CASE('6', _6)
            CASE('7', _7)
            CASE('8', _8)
            CASE('9', _9)
            CASE('=', equal, PhysicalKey::keypad_equal)
            CASE('-', minus, PhysicalKey::keypad_subtract)
            CASE('+', plus, PhysicalKey::keypad_add)
            CASE(']', right_bracket)
            CASE('[', left_bracket)
            CASE('\'', apostrophe)
            CASE(';', semicolon)
            CASE('\\', backslash)
            CASE(',', comma, PhysicalKey::keypad_decimal)
            CASE('/', slash)
            CASE('.', period, PhysicalKey::keypad_decimal)
            CASE('^', circumflex)
            CASE('`', grave_accent)
            CASE('#', hash)
            CASE('?', unknown)
            CASE('_', minus)
            CASE(':', period)
            CASE('<', left_arrow)
            CASE('>', left_arrow)
        }
    }
    return result;
}

static ModFlags translate_mod_flags(NSEventModifierFlags flags) {
    using enum ModFlags;
    ModFlags result = none;
#define MAP_FLAG(native_flag, our_flag)                                        \
    if (flags & native_flag) result |= ModFlags::our_flag;
    MAP_FLAG(NSEventModifierFlagCapsLock, caps_lock)
    MAP_FLAG(NSEventModifierFlagShift, shift)
    MAP_FLAG(NSEventModifierFlagControl, control)
    MAP_FLAG(NSEventModifierFlagOption, alt)
    MAP_FLAG(NSEventModifierFlagCommand, super)
    MAP_FLAG(NSEventModifierFlagNumericPad, num_lock)
#undef MAP_FLAG
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
    auto [key, physical] = translate_key_code(event);
    switch (event.type) {
    case NSEventTypeKeyDown:
        return KeyDownEvent(mod_flags, key, physical, event.isARepeat);
    case NSEventTypeKeyUp:
        return KeyUpEvent(mod_flags, key, physical);
    default:
        std::terminate();
    }
}

static InputEventUnion make_modifiers_changed_event(NSEvent* event) {
    ModFlags mod_flags = translate_mod_flags(event.modifierFlags);
    return ModifiersChangedEvent(mod_flags);
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
    case NSEventTypeFlagsChanged:
        g_callback_map.invoke_all(make_modifiers_changed_event(event));
        break;
    default:
        break;
    }
    return event;
}

static void create_global_monitors() {
    NSEventMask mask = 0;
    mask |= NSEventMaskMouseMoved;
    mask |= NSEventMaskLeftMouseUp;
    mask |= NSEventMaskRightMouseUp;
    mask |= NSEventMaskOtherMouseUp;
    mask |= NSEventMaskLeftMouseDragged;
    mask |= NSEventMaskRightMouseDragged;
    mask |= NSEventMaskOtherMouseDragged;
    mask |= NSEventMaskKeyDown;
    mask |= NSEventMaskKeyUp;
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