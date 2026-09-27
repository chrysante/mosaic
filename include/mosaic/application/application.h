#pragma once

#include <mosaic/application/window_desc.h>

namespace mosaic {

class Application;
class Window;
class EventSystem;
class EventEmitter;
class EventReceiver;

namespace internal {
void run_one_frame(Application& app);
}

///
enum class RunLoopMode { Wait, Poll };

///
void run_application(Application& app);

///
class Application {
    friend void mosaic::run_application(class Application&);
    friend void internal::run_one_frame(Application&);

public:
    virtual ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;

    ///
    Window* create_window(WindowDesc const& desc);

    ///
    Window* add_window(std::unique_ptr<Window> window);

    ///
    Window* main_window() {
        return const_cast<Window*>(
            static_cast<Application const&>(*this).main_window());
    }

    ///
    Window const* main_window() const;

    /// Creates an `EventEmitter` that emits to the applications event system
    EventEmitter make_emitter();

    /// Creates an `EventReceiver` that listens to the applications event system
    EventReceiver make_receiver();

protected:
    Application();

    ///
    void set_run_loop_mode(RunLoopMode mode);

private:
    // Customization points

    virtual void frame() {}

    // Internals

    struct Data;
    struct PlatformData;

    static PlatformData* create_platform_data();
    static void destroy_platform_data(PlatformData*);

    void platform_init();
    void platform_run();

    Window* create_main_window();
    void create_main_window_platform(Window* main_window);

    void imgui_init(Window* main_window);
    void imgui_init_platform(Window* main_window);
    void imgui_begin_frame();
    void imgui_begin_frame_platform();
    void imgui_end_frame();
    void imgui_end_frame_platform();

    void do_run();

    Data* m_data;
    PlatformData* m_platform_data;
};

} // namespace mosaic