#pragma once

#include <filesystem>
#include <span>

#include <vml/vml.hpp>

#include <mosaic/support/export.h>

namespace mosaic {

class Window;

/// Provides user callbacks for window events
class MOSAIC_EXPORT WindowDelegate {
public:
    virtual ~WindowDelegate() = default;

#if 0
    /// Called on text input
    virtual void on_text_input([[maybe_unused]] unsigned code) {}

    /// Called when the user drops files into the window
    virtual void on_file_drop(
        [[maybe_unused]] std::span<std::filesystem::path const> paths) {}
#endif

    /// Called when the window position changes
    virtual void on_move([[maybe_unused]] vml::int2 new_position) {}

    /// Called when the user starts moving the window
    virtual void on_move_start() {}

    /// Called when the window size changes
    virtual void on_resize([[maybe_unused]] vml::int2 new_size) {}

    /// Called when the window is focused
    virtual void on_focus() {}

    /// Called when the window loses focus
    virtual void on_focus_loss() {}

    /// Called when the user requests to close the window, e.g. by clicking the
    /// close button
    virtual bool should_close() { return true; }

    /// Called just before the window is destroyed
    virtual void on_destroy() {}

    /// Called when the content scale factor changes
    virtual void
    on_content_scale_change([[maybe_unused]] vml::float2 new_content_scale) {}

protected:
    /// \Returns the window that this delegate is attached to
    Window* window() const { return m_window; };

private:
    friend class Window;

    Window* m_window = nullptr;
};

} // namespace mosaic