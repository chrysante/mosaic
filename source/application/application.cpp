#include "application/application.h"

#include <cassert>
#include <functional>
#include <memory>

#include <imgui.h>

#include "application/application_data.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "service/event_system.h" // IWYU pragma: keep
#include "service/service_context.h"

using namespace mosaic;

void mosaic::run_application(Application& app) { app.do_run(); }

Application::Data::Data()
    : event_receiver(service_context.make_event_receiver())
    , ui_context{.font_manager = service_context.font_manager()}
    , view_manager(service_context) {}

Application::Application()
    : m_data(new Data())
    , m_platform_data(create_platform_data()) {
    m_data->input_callback_id = register_global_input_callback(
        std::bind_front(&Application::on_input, this));
    platform_init();
    auto* main_window = create_main_window();
    imgui_init(main_window);
}

Application::~Application() {
    platform_shutdown();
    unregister_global_input_callback(m_data->input_callback_id);
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

ServiceContext& Application::service_context() {
    return m_data->service_context;
}

Input& Application::input() { return m_data->input; }

CommandPalette& Application::command_palette() {
    return m_data->command_palette;
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
    auto& fm = service_context().font_manager();
    fm.set_font_atlas(io.Fonts);
    io.UserData = &m_data->ui_context;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.FontDefault = fm.get_font(FontDesc::ui_default());
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
    app.do_frame();
    app.imgui_end_frame();
}

void Application::do_run() { platform_run(); }

void Application::on_input(InputEvent event, void* native_event) {
    input_update(m_data->input, event);
    for (auto& window : m_data->windows)
        window->on_input(event, native_event);
    m_data->command_palette.on_input(event);
    m_data->service_context.m_event_system->publish(event);
}

void Application::do_frame() {
    frame();
    m_data->view_manager.display();
}