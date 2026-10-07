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

/// Physical keyboard key
enum class PhysicalKey {
#define MOSAIC_PHYSICAL_KEYS 1
#define MOSAIC_INPUT_KEY(key, label) key,
#include <mosaic/application/input.def>
};

std::string MOSAIC_EXPORT to_string(PhysicalKey key);

std::ostream& MOSAIC_EXPORT operator<<(std::ostream& ostream, PhysicalKey key);

/// Logical keyboard key
enum class Key {
#define MOSAIC_PHYSICAL_KEYS 0
#define MOSAIC_INPUT_KEY(key, label) key,
#include <mosaic/application/input.def>
};

std::string MOSAIC_EXPORT to_string(Key key);

std::ostream& MOSAIC_EXPORT operator<<(std::ostream& ostream, Key key);

/// List of all known mouse buttons
enum struct MouseButton {
#define MOSAIC_INPUT_MOUSE_BUTTON(button, label) button,
#include <mosaic/application/input.def>
};

std::string MOSAIC_EXPORT to_string(MouseButton button);

std::ostream& MOSAIC_EXPORT operator<<(std::ostream& ostream,
                                       MouseButton button);

/// Utility class that captures all input and stores it for easy access during a
/// frame. An object of this class is accessible via the application and can be
/// useful if listening to input events is too tedious
class MOSAIC_EXPORT Input {
public:
    /// \returns the current mouse position in screen coordinates
    vml::float2 mouse_position() const { return m_mouse_position; }

    /// \returns the current mouse position in the coordinates of \p window
    vml::float2 mouse_position_in_window(Window const& window) const;

    /// \returns the offset between the mouse position this frame and last frame
    vml::float2 mouse_offset() const { return m_mouse_offset; }

    /// \returns the current scroll offset
    vml::float2 scroll_offset() const { return m_scroll_offset; }

    /// \returns `true` if \p button is currently pressed
    bool mouse_down(MouseButton button) const {
        return m_mouse_button_down[(size_t)button];
    }

    /// \returns `true` if \p Key is currently pressed
    bool key_down(PhysicalKey key) const { return m_key_down[(size_t)key]; }

    /// \returns a bitfield of the currently pressed modifier keys
    ModFlags mod_flags() const { return m_mod_flags; }

    struct Impl;

private:
    ModFlags m_mod_flags = ModFlags::none;
    vml::float2 m_mouse_position = 0;
    vml::float2 m_mouse_offset = 0;
    vml::float2 m_scroll_offset = 0;
    std::array<int, enum_count<PhysicalKey>> m_key_down{};
    std::array<int, enum_count<MouseButton>> m_mouse_button_down{};
};

} // namespace mosaic
