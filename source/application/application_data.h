#pragma once

#include <memory>
#include <vector>

#include "application/application.h"
#include "application/event_receiver.h"
#include "application/font_manager.h"
#include "dispatch/dispatcher.h"
#include "dispatch/thread_pool.h"
#include "gui/ui_context.h"

namespace mosaic {

struct Application::Data {
    Data();

    std::vector<std::unique_ptr<Window>> windows;
    RunLoopMode run_loop_mode = RunLoopMode::Wait;
    std::shared_ptr<EventSystem> event_system;
    EventReceiver event_receiver;
    ThreadPool thread_pool;
    WorkDispatcher dispatcher;
    FontManager font_manager;
    UIContext ui_context;
};

} // namespace mosaic