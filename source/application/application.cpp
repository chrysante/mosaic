#include "application/application.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <functional>
#include <memory>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "application/application_impl.h"
#include "application/debug_tools.h"
#include "application/directories.h"
#include "application/frame_scheduler.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_delegate.h"
#include "application/window_impl.h"
#include "gui/appearance_internal.h"
#include "service/event_system.h" // IWYU pragma: keep
#include "service/service_context.h"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;

using internal::ApplicationImpl;

// =============================================================================
// Application interface
// =============================================================================

Application::Application()
    : m_impl(ApplicationImpl::create_impl(*this)) {
    m_impl->init();
    // Request a couple of frames when the app starts so ImGui gets a chance to
    // setup correctly.
    m_impl->frame_scheduler->request_frames(60);
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
    command_palette().add({
        .name = "Toggle Debug Overlays",
        .key_comb = {
            .key = Key::D, 
            .mod_flags = ModFlags::shift | ModFlags::alt
        },
        .command = [] { toggle_want_debug_overlays(); }
    });
// clang-format on
#endif
}

Application::~Application() {
    m_impl->shutdown_platform();
    unregister_global_input_callback(m_impl->input_callback_id);
}

namespace {

struct ApplicationWindowDelegate : WindowDelegate {
    explicit ApplicationWindowDelegate(ApplicationImpl& app_impl)
        : app_impl(app_impl) {}

    void on_resize(vml::int2) override {
        app_impl.frame_scheduler->request_frames(2);
    }

    ApplicationImpl& app_impl;
};

} // namespace

Window* Application::create_window(WindowDesc const& desc) {
    auto window = internal::WindowImpl::create_window(desc);
    register_window_for_input(*window);
    window->add_delegate(std::make_unique<ApplicationWindowDelegate>(*m_impl));
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

void Application::run_continuously(bool value) {
    if (value) m_impl->frame_scheduler->run();
    else m_impl->frame_scheduler->stop_running();
}

nlohmann::json Application::save_settings() const {
    return nlohmann::json::object();
}

// =============================================================================
// ApplicationImpl interface
// =============================================================================

ApplicationImpl::ApplicationImpl(Application& app)
    : app(app)
    , frame_scheduler(FrameScheduler::create([this] { run_one_frame(); }))
    , event_receiver(service_context.make_event_receiver())
    , ui_context{.font_manager = service_context.font_manager()}
    , view_manager(service_context)
#ifndef NDEBUG
    , debugger(std::make_unique<ApplicationDebugger>(app))
#endif
{
}

ApplicationImpl::~ApplicationImpl() = default;

void ApplicationImpl::init() {
    input_callback_id = register_global_input_callback(
        std::bind_front(&ApplicationImpl::on_input, this));
    init_platform();
    create_main_window();
    imgui_init();
}

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
    auto* window = app.create_window(
        {.position = {200, 200}, .size = {800, 600}, .title = "MAIN WINDOW"});
    window->add_delegate(std::make_unique<AppWindowDelegate>(app));
    create_main_window_platform(*window);
    return window;
}

void ApplicationImpl::run_one_frame() {
    save_settings_update();
    imgui_begin_frame();
    do_frame();
    imgui_end_frame();
}

static std::filesystem::path imgui_ini_file() {
    return config_directory() / "imgui.ini";
}

static std::filesystem::path config_json_file() {
    return config_directory() / "config.json";
}

void ApplicationImpl::save_settings_update() {
    auto& io = ImGui::GetIO();
    if (!io.WantSaveIniSettings) return;
    ImGui::SaveIniSettingsToDisk(imgui_ini_file().c_str());
    save_app_settings_to_disk();
    io.WantSaveIniSettings = false;
}

void ApplicationImpl::save_app_settings_to_disk() {
    auto const object = serialize_settings();
    auto const path = config_json_file();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        Logger::error("Failed to create config directory: ", ec.message());
        return;
    }
    std::ofstream file(path);
    if (!file) {
        Logger::error("Failed to open config file for writing: ", path);
        return;
    }
    Logger::trace("Writing config file: ", path);
    file << object.dump(4) << '\n';
    if (!file) {
        Logger::error("Failed to write config file: ", path);
    }
}

void ApplicationImpl::load_app_settings_from_disk() {
    auto const path = config_json_file();
    Logger::trace("Reading config file: ", path);
    std::ifstream file(path);
    if (!file) {
        Logger::warn("Failed to read config file: ", path);
        return;
    }
    try {
        auto const object = nlohmann::json::parse(file);
        deserialize_settings(object);
    } catch (const nlohmann::json::parse_error& e) {
        Logger::error("Failed to parse config file ", path, ": ", e.what());
    }
}

namespace config_key {

static constexpr std::string_view application = "application";
static constexpr std::string_view views = "views";

} // namespace config_key

nlohmann::json ApplicationImpl::serialize_settings() {
    auto object = nlohmann::json::object();
    object[config_key::application] = app.save_settings();
    object[config_key::views] = view_manager.save_settings();
    return object;
}

void ApplicationImpl::deserialize_settings(nlohmann::json const& base) {
    if (auto* settings = json_find(base, config_key::application))
        app.load_settings(*settings);
    if (auto* settings = json_find(base, config_key::views))
        view_manager.load_settings(*settings);
}

void ApplicationImpl::imgui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    auto& fm = app.service_context().font_manager();
    fm.set_font_atlas(io.Fonts);
    // Hard-code this here for now
    ImGui::GetStyle().FontSizeBase = 17;
    update_style(/* force: */ true);

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.UserData = &ui_context;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;
    io.FontDefault = fm.get_font(FontDesc::ui_default());
    io.IniFilename = nullptr;

    imgui_init_platform();
    ImGui::LoadIniSettingsFromDisk(imgui_ini_file().c_str());
    load_app_settings_from_disk();
}

void ApplicationImpl::imgui_begin_frame() {
    update_style();
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
    frame_scheduler->request_frames(3);
}

void ApplicationImpl::do_frame() {
    service_context.m_dispatcher.process_main_thread_tasks();
    app.frame();
    view_manager.display();
#ifndef NDEBUG
    debugger->display();
#endif
    input_end_frame(input);
}