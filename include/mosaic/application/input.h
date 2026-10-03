#pragma once

#include <iosfwd>
#include <string>

#include <utl/common.hpp>
#include <vml/vml.hpp>

#include <mosaic/support/enum.h>
#include <mosaic/support/export.h>

namespace mosaic {

class Window;
struct InputEvent;

/// Flags type to indicate if modifier keys are pressed
enum class ModFlags : unsigned {
    none = 0u,
    caps_lock = 1u << 0,
    shift = 1u << 1,
    control = 1u << 2,
    alt = 1u << 3,
    super = 1u << 4,
    num_lock = 1u << 5,
};

std::string MOSAIC_EXPORT to_string(ModFlags flags);

std::ostream& MOSAIC_EXPORT operator<<(std::ostream& ostream, ModFlags flags);

UTL_ENUM_OPERATORS(ModFlags);

/// All known keyboard keys
enum class Key {
    none = 0,
    A,
    S,
    D,
    F,
    H,
    G,
    Z,
    X,
    C,
    V,
    B,
    Q,
    W,
    E,
    R,
    Y,
    T,
    O,
    U,
    I,
    P,
    L,
    J,
    K,
    N,
    M,
    num1,
    num2,
    num3,
    num4,
    num5,
    num6,
    num7,
    num8,
    num9,
    num0,
    equal,
    minus,
    plus,
    right_bracket,
    left_bracket,
    apostrophe,
    semicolon,
    backslash,
    comma,
    slash,
    period,
    circumflex,
    accent,
    grave_accent,
    hash,
    angle_bracket_open,
    keypad_decimal,
    keypad_multiply,
    keypad_add,
    num_lock,
    keypad_divide,
    keypad_enter,
    keypad_subtract,
    keypad_equal,
    keypad0,
    keypad1,
    keypad2,
    keypad3,
    keypad4,
    keypad5,
    keypad6,
    keypad7,
    keypad8,
    keypad9,
    enter,
    tab,
    space,
    backspace,
    escape,
    caps_lock,
    left_ctrl,
    left_shift,
    left_alt,
    left_super,
    Right_ctrl,
    Right_shift,
    Right_alt,
    Right_super,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    print_screen,
    menu,
    insert,
    home,
    page_up,
    Delete,
    end,
    page_down,
    left_arrow,
    right_arrow,
    down_arrow,
    up_arrow,
};

/// List of all known mouse buttons
enum struct MouseButton {
    left = 0,
    right = 1,
    other = 2,
};

std::string MOSAIC_EXPORT to_string(MouseButton button);

std::ostream& MOSAIC_EXPORT operator<<(std::ostream& ostream,
                                       MouseButton button);

/// Utility class that captures all input and stores it for easy access during a
/// frame. An object of this class is accessible via the application and can be
/// useful if listening to input events is too tedious
class MOSAIC_EXPORT Input {
public:
    /// \Returns the current mouse position in screen coordinates
    vml::float2 mouse_position() const { return m_mouse_position; }

    /// \Returns the current mouse position in the coordinates of \p window
    vml::float2 mouse_position_in_window(Window const& window) const;

    /// \Returns the offset between the mouse position this frame and last frame
    vml::float2 mouse_offset() const { return m_mouse_offset; }

    /// \Returns the current scroll offset
    vml::float2 scroll_offset() const { return m_scroll_offset; }

    /// \Returns `true` if \p button is currently pressed
    bool mouse_down(MouseButton button) const {
        return m_mouse_button_down[(size_t)button];
    }

    /// \Returns `true` if \p Key is currently pressed
    bool key_down(Key key) const { return m_key_down[(size_t)key]; }

    /// \Returns a bitfield of the currently pressed modifier keys
    ModFlags mod_flags() const { return m_mod_flags; }

    struct Impl;

private:
    ModFlags m_mod_flags = ModFlags::none;
    vml::float2 m_mouse_position = 0;
    vml::float2 m_mouse_offset = 0;
    vml::float2 m_scroll_offset = 0;
    std::array<int, enum_count<Key>> m_key_down{};
    std::array<int, enum_count<MouseButton>> m_mouse_button_down{};
};

} // namespace mosaic
