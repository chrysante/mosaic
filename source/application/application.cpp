#include "application/application.h"

#include "application/application_data.h"
#include "application/window.h"

#include <memory>

using namespace mosaic;

void mosaic::run_application(Application& app) {
    app.do_run();
}

Application::Application()
    : m_data(new Data{})
    , m_platform_data(create_platform_data()) {
    platform_init();
}

Application::~Application() {
    destroy_platform_data(m_platform_data);
}

Window* Application::create_window(WindowDesc const& desc) {
    auto window = mosaic::create_window(desc);
    return add_window(std::move(window));
}

Window* Application::add_window(std::unique_ptr<Window> window) {
    auto* result = window.get();
    m_data->windows.push_back(std::move(window));
    return result;
}

void Application::set_run_loop_mode(RunLoopMode mode) {
    m_data->run_loop_mode = mode;
}

void Application::do_run() {
    while (true) {
        platform_poll_events();
    }
}