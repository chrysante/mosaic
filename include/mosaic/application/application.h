#pragma once

#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/application/window_desc.h>
#include <mosaic/support/export.h>

namespace mosaic {

class CommandPalette;
class Input;
class ServiceContext;
class Window;

namespace internal { struct ApplicationImpl; }

///
enum class RunLoopMode {
    Poll, /// Continuously process frames, suitable for animation, games, and
          /// real-time rendering.

    Wait /// Sleep until an event or an explicit request to refresh arrives,
         /// then process the necessary frames.
};

///
class MOSAIC_EXPORT Application {
public:
    virtual ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;

    ///
    Window* create_window(WindowDesc const& desc);

    ///
    void destroy_window(Window* window);

    ///
    Window* main_window() {
        return const_cast<Window*>(
            static_cast<Application const&>(*this).main_window());
    }

    ///
    Window const* main_window() const;

    /// \returns the applications service context
    ServiceContext& service_context();

    /// \returns the applications input manager
    Input& input();

    /// \returns the applications main command palette
    CommandPalette& command_palette();

    ///
    void quit();

    ///
    int run();

    RunLoopMode run_loop_mode() const;

    ///
    void set_run_loop_mode(RunLoopMode mode);

protected:
    Application();

private:
    // Customization points
    virtual void frame() {}
    virtual nlohmann::json save_settings() const;
    virtual void load_settings(nlohmann::json const&) {}

    friend struct internal::ApplicationImpl;

    std::unique_ptr<internal::ApplicationImpl> m_impl;
};

} // namespace mosaic