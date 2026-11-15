#include "application/application.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <memory>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "application/application_impl.h"
#include "application/debug_settings.h"
#include "application/debug_tools.h"
#include "application/directories.h"
#include "application/frame_scheduler.h"
#include "application/input.h"
#include "application/input_event.h"
#include "application/input_internal.h"
#include "application/window.h"
#include "application/window_delegate.h"
#include "application/window_desc.h"
#include "application/window_impl.h"
#include "application/window_system.h"
#include "gui/appearance_internal.h"
#include "gui/font.h"
#include "service/command_system.h"
#include "service/event_system.h" // IWYU pragma: keep
#include "service/icon_manager.h"
#include "service/service_context.h"
#include "support/draw_utils_internal.h" // IWYU pragma: keep
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
    set_debug_command_palette(command_palette());
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

Window* Application::create_window(WindowDesc const& desc) {
    return m_impl->window_system.create_window(desc);
}

void Application::destroy_window(Window* window) {
    m_impl->window_system.destroy_window(window);
}

Window const* Application::main_window() const {
    assert(m_impl->window_system.num_windows() > 0);
    return m_impl->window_system.windows().front();
}

ServiceContext& Application::service_context() {
    return m_impl->service_context;
}

ToolManager& Application::tool_manager() { return m_impl->tool_manager; }

Input& Application::input() { return m_impl->input; }

CommandPalette& Application::command_palette() {
    return m_impl->command_palette;
}

int Application::run() { return m_impl->run_platform(); }

nlohmann::json Application::save_settings() const {
    return nlohmann::json::object();
}

// =============================================================================
// ApplicationImpl interface
// =============================================================================

namespace {

struct WindowSystemDelegate final : WindowSystem::AppDelegate {
    explicit WindowSystemDelegate(ApplicationImpl& app_impl)
        : app_impl(app_impl) {}

    void request_frames() override {
        app_impl.frame_scheduler->request_frames(3);
    }

    ApplicationImpl& app_impl;
};

} // namespace

ApplicationImpl::ApplicationImpl(Application& app)
    : app(app)
    , frame_scheduler(FrameScheduler::create([this] { run_one_frame(); }))
    , window_system(std::make_unique<WindowSystemDelegate>(*this))
    , run_mode_controller(*this)
    , event_receiver(service_context.make_event_receiver())
    , ui_context{.font_manager = service_context.font_manager(),
                 .icon_manager = service_context.icon_manager(),
                 .style_system = service_context.style_system(),
                 .run_mode_controller = run_mode_controller}
    , tool_manager(service_context)
#ifndef NDEBUG
    , debugger(std::make_unique<ApplicationDebugger>(app))
#endif
{
}

ApplicationImpl::~ApplicationImpl() = default;

void ApplicationImpl::init() {
    // Init fundamental systems
    input_callback_id = register_global_input_callback(
        std::bind_front(&ApplicationImpl::on_input, this));
    init_platform();
    create_main_window();
    imgui_init();

    // Restore config state
    restore_config();

    // Init higher-level systems
    service_context.style_system().init();
}

namespace {

struct MainWindowDelegate : WindowDelegate {
    explicit MainWindowDelegate(Application& app)
        : app(app) {}

    void on_destroy() override { app.quit(); }

    Application& app;
};

} // namespace

Window* ApplicationImpl::create_main_window() {
    assert(window_system.num_windows() == 0);
    auto* window = window_system.create_window(
        {.geometry = {.position = {200, 200}, .size = {800, 600}},
         .title = "MAIN WINDOW"});
    window->add_delegate(std::make_unique<MainWindowDelegate>(app));
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

void ApplicationImpl::restore_config() {
    ImGui::LoadIniSettingsFromDisk(imgui_ini_file().c_str());
    load_app_settings_from_disk();
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

template <> struct nlohmann::adl_serializer<WindowGeometry> {
    static void to_json(json& j, WindowGeometry const& geometry) {
        j["position"] = geometry.position;
        j["size"] = geometry.size;
    }
    static void from_json(json const& j, WindowGeometry& geometry) {
        j.at("position").get_to(geometry.position);
        j.at("size").get_to(geometry.size);
    }
};

namespace config_key {

static constexpr std::string_view application = "application";
static constexpr std::string_view window_geometry = "window_geometry";
static constexpr std::string_view tools = "tools";
static constexpr std::string_view style = "style";

} // namespace config_key

nlohmann::json ApplicationImpl::serialize_settings() {
    auto object = nlohmann::json::object();
    object[config_key::application] = app.save_settings();
    if (auto* main_window = app.main_window())
        object[config_key::window_geometry] = main_window->geometry();
    object[config_key::tools] = tool_manager.save_config();
    object[config_key::style] = service_context.style_system().save_config();
    return object;
}

void ApplicationImpl::deserialize_settings(nlohmann::json const& base) {
    if (auto* settings = json_find(base, config_key::application))
        app.load_settings(*settings);
    if (auto geo = json_find<WindowGeometry>(base, config_key::window_geometry))
        if (auto* main_window = app.main_window())
            main_window->set_geometry(geo.value());
    if (auto* config = json_find(base, config_key::tools))
        tool_manager.load_config(*config);
    if (auto* config = json_find(base, config_key::style))
        service_context.style_system().load_config(*config);
}

void ApplicationImpl::imgui_init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.UserData = &ui_context;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;
    io.IniFilename = nullptr;

    auto& fm = app.service_context().font_manager();
    fm.set_font_atlas(io.Fonts);
    init_font(17);
    load_default_icons(service_context.icon_manager());
    update_style(/* force: */ true);

    imgui_init_platform();
}

void ApplicationImpl::init_font(float base_size_absolute) {
    FontDesc desc = {.weight = FontWeight::regular, .style = FontStyle::roman};
    FontContext font_ctx{.desc = desc, .size = base_size_absolute};
    ui_context.font_stack.push(font_ctx);
    auto& io = ImGui::GetIO();
    io.FontDefault = service_context.font_manager().get_font(desc);
    auto& style = ImGui::GetStyle();
    style.FontSizeBase = base_size_absolute;
}

void ApplicationImpl::imgui_begin_frame() {
    update_style();
    imgui_begin_frame_platform();
    ImGui::NewFrame();
}

void ApplicationImpl::imgui_copy_item_hovered_to_windows() {
    bool item_hovered = ImGui::IsAnyItemHovered();
    for (auto* window : window_system.windows())
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
    window_system.update();
    app.frame();
    tool_manager.update();
    service_context.panel_manager().display();
    service_context.command_system().process(app);
#ifndef NDEBUG
    debugger->display();
#endif
    input_end_frame(input);
}