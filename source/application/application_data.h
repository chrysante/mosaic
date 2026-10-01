#pragma once

#include <memory>
#include <vector>

#include "application/application.h"
#include "application/input.h"
#include "gui/command_palette.h"
#include "gui/ui_context.h"
#include "gui/view_manager.h"
#include "service/event_receiver.h"
#include "service/service_context.h"

namespace mosaic {

struct Application::Data {
    Data();

    ServiceContext service_context;
    std::vector<std::unique_ptr<Window>> windows;
    RunLoopMode run_loop_mode = RunLoopMode::Wait;
    EventReceiver event_receiver;
    UIContext ui_context;
    Input input;
    int input_callback_id = 0;
    CommandPalette command_palette;
    ViewManager view_manager;
};

} // namespace mosaic