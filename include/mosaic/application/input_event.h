// Screen and window coordinates:
//
// (0, 0)┌─ Main Display ───────────────────────────────────┐
//       │                                                  │
//       │  window.position                                 │
//       │       ->┌ Window ────────┐                       │
//       │         │(0, 0)          │                       │
//       │         │                │                       │
//       │         │                │                       │
//       │         │     window.size│                       │
//       │         └────────────────┘                       │
//       │                          ^                       │
//       │                   window.position + window.size  │
//       │                                                  │
//       │                                                  │
//       └──────────────────────────────────────────────────┘ Display size

// Input Event Class Hierarchy:
//
//     InputEvent
//     ├── MouseEvent
//     │   ├── MouseDownEvent
//     │   │   ├── LeftMouseDownEvent
//     │   │   ├── RightMouseDownEvent
//     │   │   └── OtherMouseDownEvent
//     │   ├── MouseUpEvent
//     │   │   ├── LeftMouseUpEvent
//     │   │   ├── RightMouseUpEvent
//     │   │   └── OtherMouseUpEvent
//     │   ├── MouseDragEvent
//     │   │   ├── LeftMouseDragEvent
//     │   │   ├── RightMouseDragEvent
//     │   │   └── OtherMouseDragEvent
//     │   ├── MouseMoveEvent
//     │   ├── ScrollEvent
//     │   └── MagnificationEvent
//     └── KeyEvent
//         ├── KeyDownEvent
//         └── KeyUpEvent
//

#pragma once

#include <csp.hpp>
#include <utl/common.hpp>
#include <vml/vml.hpp>

#include <mosaic/application/input.h>

namespace mosaic {

struct InputEvent;
struct MouseEvent;
struct MouseDownEvent;
struct MouseUpEvent;
struct MouseDragEvent;
struct LeftMouseDownEvent;
struct LeftMouseUpEvent;
struct LeftMouseDragEvent;
struct RightMouseDownEvent;
struct RightMouseUpEvent;
struct RightMouseDragEvent;
struct OtherMouseDownEvent;
struct OtherMouseUpEvent;
struct OtherMouseDragEvent;
struct MouseMoveEvent;
struct ScrollEvent;
struct MagnificationEvent;
struct KeyEvent;
struct KeyDownEvent;
struct KeyUpEvent;

namespace internal {

enum class InputEventType {
    InputEvent,
    MouseEvent,
    MouseDownEvent,
    MouseUpEvent,
    MouseDragEvent,
    LeftMouseDownEvent,
    LeftMouseUpEvent,
    LeftMouseDragEvent,
    RightMouseDownEvent,
    RightMouseUpEvent,
    RightMouseDragEvent,
    OtherMouseDownEvent,
    OtherMouseUpEvent,
    OtherMouseDragEvent,
    MouseMoveEvent,
    ScrollEvent,
    MagnificationEvent,
    KeyEvent,
    KeyDownEvent,
    KeyUpEvent,
};

}

} // namespace mosaic

// clang-format off
CSP_DEFINE(mosaic::InputEvent,          mosaic::internal::InputEventType::InputEvent,          void,                                 Abstract)
CSP_DEFINE(mosaic::MouseEvent,          mosaic::internal::InputEventType::MouseEvent,          mosaic::InputEvent,     Abstract)
CSP_DEFINE(mosaic::MouseDownEvent,      mosaic::internal::InputEventType::MouseDownEvent,      mosaic::MouseEvent,     Abstract)
CSP_DEFINE(mosaic::MouseUpEvent,        mosaic::internal::InputEventType::MouseUpEvent,        mosaic::MouseEvent,     Abstract)
CSP_DEFINE(mosaic::MouseDragEvent,      mosaic::internal::InputEventType::MouseDragEvent,      mosaic::MouseEvent,     Abstract)
CSP_DEFINE(mosaic::LeftMouseDownEvent,  mosaic::internal::InputEventType::LeftMouseDownEvent,  mosaic::MouseDownEvent, Concrete)
CSP_DEFINE(mosaic::LeftMouseUpEvent,    mosaic::internal::InputEventType::LeftMouseUpEvent,    mosaic::MouseUpEvent,   Concrete)
CSP_DEFINE(mosaic::LeftMouseDragEvent,  mosaic::internal::InputEventType::LeftMouseDragEvent,  mosaic::MouseDragEvent, Concrete)
CSP_DEFINE(mosaic::RightMouseDownEvent, mosaic::internal::InputEventType::RightMouseDownEvent, mosaic::MouseDownEvent, Concrete)
CSP_DEFINE(mosaic::RightMouseUpEvent,   mosaic::internal::InputEventType::RightMouseUpEvent,   mosaic::MouseUpEvent,   Concrete)
CSP_DEFINE(mosaic::RightMouseDragEvent, mosaic::internal::InputEventType::RightMouseDragEvent, mosaic::MouseDragEvent, Concrete)
CSP_DEFINE(mosaic::OtherMouseDownEvent, mosaic::internal::InputEventType::OtherMouseDownEvent, mosaic::MouseDownEvent, Concrete)
CSP_DEFINE(mosaic::OtherMouseUpEvent,   mosaic::internal::InputEventType::OtherMouseUpEvent,   mosaic::MouseUpEvent,   Concrete)
CSP_DEFINE(mosaic::OtherMouseDragEvent, mosaic::internal::InputEventType::OtherMouseDragEvent, mosaic::MouseDragEvent, Concrete)
CSP_DEFINE(mosaic::MouseMoveEvent,      mosaic::internal::InputEventType::MouseMoveEvent,      mosaic::MouseEvent,     Concrete)
CSP_DEFINE(mosaic::ScrollEvent,         mosaic::internal::InputEventType::ScrollEvent,         mosaic::MouseEvent,     Concrete)
CSP_DEFINE(mosaic::MagnificationEvent,  mosaic::internal::InputEventType::MagnificationEvent,  mosaic::MouseEvent,     Concrete)
CSP_DEFINE(mosaic::KeyEvent,            mosaic::internal::InputEventType::KeyEvent,            mosaic::InputEvent,     Abstract)
CSP_DEFINE(mosaic::KeyDownEvent,        mosaic::internal::InputEventType::KeyDownEvent,        mosaic::KeyEvent,       Concrete)
CSP_DEFINE(mosaic::KeyUpEvent,          mosaic::internal::InputEventType::KeyUpEvent,          mosaic::KeyEvent,       Concrete)
// clang-format on

namespace mosaic {

struct InputEvent : public csp::base_helper<InputEvent> {
    ModFlags modifier_flags;

protected:
    InputEvent(internal::InputEventType type, ModFlags mod_flags)
        : base_helper(type)
        , modifier_flags(mod_flags) {}
};

/// Common data of all mouse events
struct MouseEvent : InputEvent {
    /// The mouse location in window coordinates
    vml::double2 location_in_window;

    /// The mouse location in screen coordinates
    vml::double2 location_on_screen;

protected:
    MouseEvent(internal::InputEventType type, ModFlags mod_flags,
               vml::double2 location_in_window, vml::double2 location_on_screen)
        : InputEvent(type, mod_flags)
        , location_in_window(location_in_window)
        , location_on_screen(location_on_screen) {}
};

/// Event generated by mouse clicks
struct MouseDownEvent : MouseEvent {
    /// The mouse button clicked
    MouseButton button;

    /// The number of repeated clicks
    long click_count;

protected:
    MouseDownEvent(internal::InputEventType type, ModFlags mod_flags,
                   vml::double2 location_in_window,
                   vml::double2 location_on_screen, MouseButton button,
                   long click_count)
        : MouseEvent(type, mod_flags, location_in_window, location_on_screen)
        , button(button)
        , click_count(click_count) {}
};

/// Event generated when mouse clicks are released
struct MouseUpEvent : MouseEvent {
    /// The mouse button released
    MouseButton button;

protected:
    MouseUpEvent(internal::InputEventType type, ModFlags mod_flags,
                 vml::double2 location_in_window,
                 vml::double2 location_on_screen, MouseButton button)
        : MouseEvent(type, mod_flags, location_in_window, location_on_screen)
        , button(button) {}
};

/// Event generated by mouse clicking and dragging
struct MouseDragEvent : MouseEvent {
    /// The mouse button clicked
    MouseButton button;

    /// The offset between this and the last mouse event
    vml::double2 offset;

protected:
    MouseDragEvent(internal::InputEventType type, ModFlags mod_flags,
                   vml::double2 location_in_window,
                   vml::double2 location_on_screen, MouseButton button,
                   vml::double2 offset)
        : MouseEvent(type, mod_flags, location_in_window, location_on_screen)
        , button(button)
        , offset(offset) {}
};

struct LeftMouseDownEvent : MouseDownEvent {
    LeftMouseDownEvent(ModFlags mod_flags, vml::double2 location_in_window,
                       vml::double2 location_on_screen, long click_count)
        : MouseDownEvent(internal::InputEventType::LeftMouseDownEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::left, click_count) {}
};

struct LeftMouseUpEvent : MouseUpEvent {
    LeftMouseUpEvent(ModFlags mod_flags, vml::double2 location_in_window,
                     vml::double2 location_on_screen)
        : MouseUpEvent(internal::InputEventType::LeftMouseUpEvent, mod_flags,
                       location_in_window, location_on_screen,
                       MouseButton::left) {}
};

struct LeftMouseDragEvent : MouseDragEvent {
    LeftMouseDragEvent(ModFlags mod_flags, vml::double2 location_in_window,
                       vml::double2 location_on_screen, vml::double2 offset)
        : MouseDragEvent(internal::InputEventType::LeftMouseDragEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::left, offset) {}
};

struct RightMouseDownEvent : MouseDownEvent {
    RightMouseDownEvent(ModFlags mod_flags, vml::double2 location_in_window,
                        vml::double2 location_on_screen, long click_count)
        : MouseDownEvent(internal::InputEventType::RightMouseDownEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::right, click_count) {}
};

struct RightMouseUpEvent : MouseUpEvent {
    RightMouseUpEvent(ModFlags mod_flags, vml::double2 location_in_window,
                      vml::double2 location_on_screen)
        : MouseUpEvent(internal::InputEventType::RightMouseUpEvent, mod_flags,
                       location_in_window, location_on_screen,
                       MouseButton::right) {}
};

struct RightMouseDragEvent : MouseDragEvent {
    RightMouseDragEvent(ModFlags mod_flags, vml::double2 location_in_window,
                        vml::double2 location_on_screen, vml::double2 offset)
        : MouseDragEvent(internal::InputEventType::RightMouseDragEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::right, offset) {}
};

struct OtherMouseDownEvent : MouseDownEvent {
    OtherMouseDownEvent(ModFlags mod_flags, vml::double2 location_in_window,
                        vml::double2 location_on_screen, long click_count)
        : MouseDownEvent(internal::InputEventType::OtherMouseDownEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::other, click_count) {}
};

struct OtherMouseUpEvent : MouseUpEvent {
    OtherMouseUpEvent(ModFlags mod_flags, vml::double2 location_in_window,
                      vml::double2 location_on_screen)
        : MouseUpEvent(internal::InputEventType::OtherMouseUpEvent, mod_flags,
                       location_in_window, location_on_screen,
                       MouseButton::other) {}
};

struct OtherMouseDragEvent : MouseDragEvent {
    OtherMouseDragEvent(ModFlags mod_flags, vml::double2 location_in_window,
                        vml::double2 location_on_screen, vml::double2 offset)
        : MouseDragEvent(internal::InputEventType::OtherMouseDragEvent,
                         mod_flags, location_in_window, location_on_screen,
                         MouseButton::other, offset) {}
};

/// Event generated by mouse movements
struct MouseMoveEvent : MouseEvent {
    /// The offset between this and the last mouse event
    vml::double2 offset;

    MouseMoveEvent(ModFlags mod_flags, vml::double2 location_in_window,
                   vml::double2 location_on_screen, vml::double2 offset)
        : MouseEvent(internal::InputEventType::MouseMoveEvent, mod_flags,
                     location_in_window, location_on_screen)
        , offset(offset) {}
};

/// Event generated by mouse or trackpad scrolling
struct ScrollEvent : MouseEvent {
    /// The scroll offset
    vml::double2 offset;

    /// `true` if this event was generated by a trackpad
    bool is_trackpad;

    ScrollEvent(ModFlags mod_flags, vml::double2 location_in_window,
                vml::double2 location_on_screen, vml::double2 offset,
                bool is_trackpad)
        : MouseEvent(internal::InputEventType::ScrollEvent, mod_flags,
                     location_in_window, location_on_screen)
        , offset(offset)
        , is_trackpad(is_trackpad) {}
};

/// "Zoom" gesture event, probably only generated by trackpads on MacOS
struct MagnificationEvent : MouseEvent {
    double magnification;

    MagnificationEvent(ModFlags mod_flags, vml::double2 location_in_window,
                       vml::double2 location_on_screen, double magnification)
        : MouseEvent(internal::InputEventType::MagnificationEvent, mod_flags,
                     location_in_window, location_on_screen)
        , magnification(magnification) {}
};

/// Common key event data
struct KeyEvent : InputEvent {
    /// The key that generated the event
    Key key;

protected:
    KeyEvent(internal::InputEventType type, ModFlags mod_flags, Key key)
        : InputEvent(type, mod_flags)
        , key(key) {}
};

struct KeyDownEvent : KeyEvent {
    ///
    int repeat_count;

    KeyDownEvent(ModFlags mod_flags, Key key, int repeat_count)
        : KeyEvent(internal::InputEventType::KeyDownEvent, mod_flags, key)
        , repeat_count(repeat_count) {}
};

struct KeyUpEvent : KeyEvent {
    KeyUpEvent(ModFlags mod_flags, Key key)
        : KeyEvent(internal::InputEventType::KeyUpEvent, mod_flags, key) {}
};

using InputEventUnion = csp::dyn_union<InputEvent>;

} // namespace mosaic
