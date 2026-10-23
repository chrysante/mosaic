#include "gui/panel_manager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include "application/input_event.h"
#include "gui/panel.h"
#include "gui/panel_registry.h"
#include "gui/panel_renderer.h"
#include "service/service_context.h"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;

namespace {

struct DeferredClose { Panel* panel; };

} // namespace

PanelManager::PanelManager(ServiceContext& service_context)
    : service_context(service_context)
    , event_receiver(service_context.make_event_receiver())
    , renderer(std::make_unique<PanelRenderer>()) {
    event_receiver.listen([this](OpenCommand const& command) {
        open_panel(command.name, command.completion);
    });
    event_receiver.listen([this](CloseAllCommand) { close_all(); });
}

PanelManager::~PanelManager() = default;

std::uint64_t PanelManager::new_instance_id(PanelClass const* type) const {
    auto map_itr = m_panels.find(type);
    if (map_itr == m_panels.end()) return 0;
    auto& map = map_itr->second;
    std::uint64_t id = 0;
    for (;; ++id)
        if (!map.contains(id)) break;
    return id;
}

Panel* PanelManager::insert_panel(PanelId panel_id,
                                  std::unique_ptr<Panel> panel) {
    assert(panel != nullptr);
    auto* result = panel.get();
    auto [_, success] = m_panels[panel_id.panel_class].emplace(
        panel_id.instance, std::move(panel));
    assert(success);
    return result;
}

Panel* PanelManager::open_panel(PanelRegistryEntry const& entry,
                                std::function<void(Panel&)> completion) {
    if (auto* existing = find_by_type(entry.panel_class())) {
        existing->take_focus();
        if (completion) completion(*existing);
        return existing;
    }
    auto panel = entry.create_panel();
    auto panel_id = PanelId{
        .panel_class = &entry.panel_class(),
        .instance = new_instance_id(&entry.panel_class()),
    };
    panel->initialize(panel_id, service_context);
    auto* result = insert_panel(panel_id, std::move(panel));
    result->take_focus();
    if (completion) completion(*result);
    return result;
}

Panel* PanelManager::open_panel(std::string_view name,
                                std::function<void(Panel&)> completion) {
    auto entry = PanelRegistry::find(name);
    if (!entry) {
        Logger::error("Panel does not exist: ", name);
        return nullptr;
    }
    return open_panel(*entry, std::move(completion));
}

Panel const* PanelManager::find_by_type(PanelClass const& panel_class) const {
    auto itr = m_panels.find(&panel_class);
    if (itr == m_panels.end()) return nullptr;
    auto& map = itr->second;
    if (map.empty()) return nullptr;
    return map.begin()->second.get();
}

Panel const* PanelManager::find_by_id(PanelId panel_id) const {
    auto map_itr = m_panels.find(panel_id.panel_class);
    if (map_itr == m_panels.end()) return nullptr;
    auto& map = map_itr->second;
    auto itr = map.find(panel_id.instance);
    if (itr == map.end()) return nullptr;
    return itr->second.get();
}

void PanelManager::close_all() {
    for (auto& [panel_class, map] : m_panels)
        for (auto& [id, view] : map)
            view->request_close();
}

void PanelManager::process_deferred_operations() {
    // Destroy all views that requested to close
    for (auto& [panel_class, map] : m_panels)
        std::erase_if(map, [](auto& p) { return p.second->close_requested(); });
}

void PanelManager::display() {
    process_deferred_operations();
    renderer->begin_frame();
    m_focused_panel = nullptr;
    for (auto& [panel_class, map] : m_panels) {
        for (auto& [instance, panel] : map) {
            auto result = renderer->render_panel(*panel);
            if (result.focused) m_focused_panel = panel.get();
        }
    }
    renderer->end_frame();
    process_deferred_operations();
}

#ifndef NDEBUG
void PanelManager::draw_debugger() {
    if (m_focused_panel)
        ImGui::Text("Focused Panel: [%u] %s",
                    (unsigned)m_focused_panel->panel_id().instance,
                    m_focused_panel->title().c_str());
    else ImGui::Text("Focused Panel: none");
    ImGui::SeparatorText("Views");
    for (auto& [panel_class, map] : m_panels) {
        if (!ImGui::TreeNodeEx(panel_class->name.c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen))
            continue;
        for (auto& [instance, panel] : map) {
            if (!panel) {
                ImGui::BulletText("[%u] NULL", (unsigned)instance);
                continue;
            }
            ImGui::PushID(static_cast<int>(instance));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("[%u] %s", (unsigned)instance, panel->title().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%p)", static_cast<void const*>(panel.get()));
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}
#endif

bool PanelManager::on_input(InputEvent& event) {
    /*
     * I would keep input routing here rather than in Panel.
     *
     * The manager knows which Panel is focused.
     *
     * This can later become:
     *
     *     if (focused && focused->on_input(event))
     *         return true;
     *
     * without Views knowing about the global input dispatcher.
     */

    (void)event;
    return false;
}

// =============================================================================
// Serialization
// =============================================================================

namespace config_key {

static constexpr std::string_view type = "type";
static constexpr std::string_view instance = "instance";
static constexpr std::string_view hidden = "layout.hidden";
static constexpr std::string_view maximized = "layout.maximized";
static constexpr std::string_view state = "state";

} // namespace config_key

nlohmann::json PanelManager::save_settings() const {
    auto root = nlohmann::json::array();
    for (auto& [panel_class, map] : m_panels) {
        for (auto& [instance, view] : map) {
            auto object = nlohmann::json::object();
            object[config_key::type] = std::string(view->type_name());
            object[config_key::instance] = view->panel_id().instance;
            object[config_key::hidden] = view->hidden();
            object[config_key::maximized] = view->maximized();
            object[config_key::state] = view->save_settings();
            root.push_back(std::move(object));
        }
    }
    return root;
}

void PanelManager::load_settings(nlohmann::json const& root) {
    m_panels.clear();
    m_focused_panel = nullptr;
    if (!root.is_array()) return;
    for (auto const& node : root) {
        if (!node.is_object()) {
            Logger::error("Failed to deserialize view: not an object");
            continue;
        }
        auto type_name = json_find<std::string>(node, config_key::type);
        auto instance = json_find<std::uint64_t>(node, config_key::instance);
        if (!type_name || !instance) {
            Logger::error(
                "Failed to deserialize view: missing or invalid type/instance");
            continue;
        }
        auto entry = PanelRegistry::find(*type_name);
        if (!entry) {
            Logger::error("Failed to deserialize view: unknown type: ",
                          *type_name);
            continue;
        }
        auto* panel_class = &entry->panel_class();
        auto view = entry->create_panel();
        PanelId const panel_id{
            .panel_class = panel_class,
            .instance = *instance,
        };
        view->initialize(panel_id, service_context);
        if (auto hidden = json_find<bool>(node, config_key::hidden))
            view->set_hidden(*hidden);
        if (auto maximized = json_find<bool>(node, config_key::maximized))
            view->set_maximized(*maximized);
        if (auto* state = json_find(node, config_key::state))
            view->load_settings(*state);
        if (find_by_id(panel_id) != nullptr) {
            Logger::error(
                "Failed to deserialize view: instance id already taken");
        } else {
            insert_panel(panel_id, std::move(view));
        }
    }
}