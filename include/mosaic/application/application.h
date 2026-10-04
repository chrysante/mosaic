#pragma once

#include <memory>

#include <mosaic/application/window_desc.h>
#include <mosaic/support/export.h>

namespace mosaic {

class CommandPalette;
class Input;
class ServiceContext;
class Window;

namespace internal { struct ApplicationImpl; }

///
enum class RunLoopMode { Wait, Poll };

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

protected:
    Application();

    ///
    void set_run_loop_mode(RunLoopMode mode);

private:
    // Customization points
    virtual void frame() {}

    friend struct internal::ApplicationImpl;

    std::unique_ptr<internal::ApplicationImpl> m_impl;
};

} // namespace mosaic