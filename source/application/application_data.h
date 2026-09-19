#pragma once

#include "application/application.h"

#include <vector>

namespace mosaic {

struct Application::Data {
    std::vector<std::unique_ptr<Window>> windows;
    RunLoopMode run_loop_mode = RunLoopMode::Wait;
};

} // namespace mosaic