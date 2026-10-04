#include "application/application.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>

#include <imgui.h>

#include "application/application_debugger.h"
#include "application/application_impl.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_delegate.h"
#include "application/window_impl.h"
#include "service/event_system.h" // IWYU pragma: keep
#include "service/service_context.h"

using namespace mosaic;

using internal::ApplicationImpl;

// =============================================================================
// Application interface
// =============================================================================

Application::Application()
    : m_impl(ApplicationImpl::create_impl(*this)) {
    m_impl->input_callback_id = register_global_input_callback(
        std::bind_front(&ApplicationImpl::on_input, m_impl.get()));
    m_impl->init_platform();
    m_impl->create_main_window();
    m_impl->imgui_init();
#ifndef NDEBUG
    // clang-format off
    command_palette().add({
        .name = "Toggle Debugger",
        .key_comb = {
            .key = Key::D, 
            .mod_flags = ModFlags::shift | ModFlags::control
        },
        .command = [d = m_impl->debugger.get()] { d->toggle_open(); }
    });
    // clang-format on
#endif
}

Application::~Application() {
    m_impl->shutdown_platform();
    unregister_global_input_callback(m_impl->input_callback_id);
}

Window* Application::create_window(WindowDesc const& desc) {
    auto window = internal::WindowImpl::create_window(desc);
    register_window_for_input(*window);
    auto* result = window.get();
    m_impl->windows.push_back(std::move(window));
    return result;
}

void Application::destroy_window(Window* window) {
    service_context().dispatcher().on_main_thread(
        [impl = m_impl.get(), window] {
            auto itr = std::ranges::find_if(
                impl->windows, [&](auto& ptr) { return ptr.get() == window; });
            assert(itr != impl->windows.end());
            impl->windows.erase(itr);
        });
}

Window const* Application::main_window() const {
    assert(!m_impl->windows.empty());
    return m_impl->windows.front().get();
}

ServiceContext& Application::service_context() {
    return m_impl->service_context;
}

Input& Application::input() { return m_impl->input; }

CommandPalette& Application::command_palette() {
    return m_impl->command_palette;
}

int Application::run() { return m_impl->run_platform(); }

void Application::set_run_loop_mode(RunLoopMode mode) {
    m_impl->run_loop_mode = mode;
}

// =============================================================================
// ApplicationImpl interface
// =============================================================================

ApplicationImpl::ApplicationImpl(Application& app)
    : app(app)
    , event_receiver(service_context.make_event_receiver())
    , ui_context{.font_manager = service_context.font_manager()}
    , view_manager(service_context)
#ifndef NDEBUG
    , debugger(std::make_unique<ApplicationDebugger>(app))
#endif
{
}

ApplicationImpl::~ApplicationImpl() = default;

namespace {

struct AppWindowDelegate : WindowDelegate {
    explicit AppWindowDelegate(Application& app)
        : app(app) {}

    void on_destroy() override { app.quit(); }

    Application& app;
};

} // namespace

Window* ApplicationImpl::create_main_window() {
    assert(windows.empty());
    auto* window =
        app.create_window({.position = {200, 200}, .size = {800, 600}});
    window->add_delegate(std::make_unique<AppWindowDelegate>(app));
    create_main_window_platform(*window);
    return window;
}

void ApplicationImpl::run_one_frame() {
    imgui_begin_frame();
    do_frame();
    imgui_end_frame();
}

void ApplicationImpl::imgui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    auto& fm = app.service_context().font_manager();
    fm.set_font_atlas(io.Fonts);
    io.UserData = &ui_context;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;
    io.FontDefault = fm.get_font(FontDesc::ui_default());
    ImGui::StyleColorsDark();
    imgui_init_platform();
}

void ApplicationImpl::imgui_begin_frame() {
    imgui_begin_frame_platform();
    ImGui::NewFrame();
}

void ApplicationImpl::imgui_copy_item_hovered_to_windows() {
    bool item_hovered = ImGui::IsAnyItemHovered();
    for (auto& window : windows)
        internal::WindowImpl::set_titlebar_controls_hovered(*window,
                                                            item_hovered);
}

void ApplicationImpl::imgui_end_frame() {
    imgui_copy_item_hovered_to_windows();
    ImGui::Render();
    imgui_end_frame_platform();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}

void ApplicationImpl::on_input(InputEventUnion const& event) {
    input_update(input, event.base());
    command_palette.on_input(event.base());
    service_context.m_event_system->publish(event);
#ifndef NDEBUG
    debugger->on_input(event);
#endif
}

void ApplicationImpl::do_frame() {
    service_context.m_dispatcher.process_main_thread_tasks();
    app.frame();
    view_manager.display();
#ifndef NDEBUG
    debugger->display();
#endif
}