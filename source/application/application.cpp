#include "application/application.h"

#include "application/application_data.h"
#include "application/window.h"

#include <cassert>
#include <imgui.h>
#include <memory>

using namespace mosaic;

void mosaic::run_application(Application& app) {
    app.do_run();
}

Application::Application()
    : m_data(new Data{})
    , m_platform_data(create_platform_data()) {
    platform_init();
    auto* main_window = create_main_window();
    imgui_init(main_window);
}

Application::~Application() {
    destroy_platform_data(m_platform_data);
    delete m_data;
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

Window const* Application::main_window() const {
    assert(!m_data->windows.empty());
    return m_data->windows.front().get();
}

void Application::set_run_loop_mode(RunLoopMode mode) {
    m_data->run_loop_mode = mode;
}

Window* Application::create_main_window() {
    assert(m_data->windows.empty());
    auto* window = create_window({.position = {200, 200}, .size = {800, 600}});
    create_main_window_platform(window);
    return window;
}

void Application::imgui_init(Window* main_window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    imgui_init_platform(main_window);
}

void Application::imgui_begin_frame() {
    imgui_begin_frame_platform();
    ImGui::NewFrame();
}

void Application::imgui_end_frame() {
    ImGui::Render();
    imgui_end_frame_platform();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

void internal::run_one_frame(Application& app) {
    app.imgui_begin_frame();
    app.frame();
    app.imgui_end_frame();
}

void Application::do_run() {
    platform_run();
}