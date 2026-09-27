#pragma once

#include "application/application.h"

#include <memory>
#include <vector>

namespace mosaic {

struct Application::Data {
    std::vector<std::unique_ptr<Window>> windows;
    RunLoopMode run_loop_mode = RunLoopMode::Wait;
    std::shared_ptr<EventSystem> event_system;
};

} // namespace mosaic