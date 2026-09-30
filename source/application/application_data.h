#pragma once

#include <memory>
#include <vector>

#include "application/application.h"
#include "application/input.h"
#include "gui/command_palette.h"
#include "gui/ui_context.h"
#include "service/dispatcher.h"
#include "service/event_receiver.h"
#include "service/font_manager.h"
#include "service/thread_pool.h"

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
    Input input;
    int input_callback_id = 0;
    CommandPalette command_palette;
};

} // namespace mosaic