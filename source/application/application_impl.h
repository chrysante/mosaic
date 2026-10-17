#pragma once

#include <memory>
#include <vector>

#include "application/application.h"
#include "application/frame_scheduler.h"
#include "application/input.h"
#include "gui/command_palette.h"
#include "gui/ui_context.h"
#include "gui/view_manager.h"
#include "service/event_receiver.h"
#include "service/service_context.h"

namespace mosaic {

#ifndef NDEBUG
class ApplicationDebugger;
#endif

struct internal::ApplicationImpl {
    ApplicationImpl(Application& app);

    virtual ~ApplicationImpl();

    static std::unique_ptr<ApplicationImpl> create_impl(Application& app);

    void init();

    Window* create_main_window();

    void run_one_frame();

    virtual void init_platform() = 0;
    virtual int run_platform() = 0;
    virtual void shutdown_platform() = 0;
    virtual void create_main_window_platform(Window&) = 0;

    void save_settings_update();
    void save_app_settings_to_disk();
    void load_app_settings_from_disk();

    nlohmann::json serialize_settings();
    void deserialize_settings(nlohmann::json const&);

    void imgui_init();
    void imgui_begin_frame();
    void imgui_copy_item_hovered_to_windows();
    void imgui_end_frame();

    virtual void imgui_init_platform() = 0;
    virtual void imgui_begin_frame_platform() = 0;
    virtual void imgui_end_frame_platform() = 0;

    void on_input(InputEventUnion const& event);
    void do_frame();

    static ApplicationImpl& get_impl(Application& app) { return *app.m_impl; }

    static ApplicationImpl const& get_impl(Application const& app) {
        return *app.m_impl;
    }

    Application& app;
    ServiceContext service_context;
    std::vector<std::unique_ptr<Window>> windows;
    std::unique_ptr<FrameScheduler> frame_scheduler;
    EventReceiver event_receiver;
    UIContext ui_context;
    Input input;
    int input_callback_id = 0;
    CommandPalette command_palette;
    ViewManager view_manager;
#ifndef NDEBUG
    std::unique_ptr<ApplicationDebugger> debugger;
#endif
};

} // namespace mosaic
