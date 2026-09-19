#pragma once

#include <mosaic/application/window_desc.h>

namespace mosaic {

class Application;
class Window;

///
enum class RunLoopMode { Wait, Poll };

///
void run_application(Application& app);

///
class Application {
    friend void mosaic::run_application(class Application&);

public:
    virtual ~Application();

    Application(Application const&) = delete;
    Application& operator=(Application const&) = delete;

    ///
    Window* create_window(WindowDesc const& desc);

    ///
    Window* add_window(std::unique_ptr<Window> window);

protected:
    Application();

    virtual void frame() {}

    ///
    void set_run_loop_mode(RunLoopMode mode);

private:
    struct Data;
    struct PlatformData;

    static PlatformData* create_platform_data();
    static void destroy_platform_data(PlatformData*);

    void platform_init();

    void platform_poll_events();

    void do_run();

    Data* m_data;
    PlatformData* m_platform_data;
};

} // namespace mosaic