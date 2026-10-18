#include "gui/view_manager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include "application/input_event.h"
#include "gui/view.h"
#include "gui/view_registry.h"
#include "gui/view_renderer.h"
#include "service/service_context.h"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;

namespace {

struct DeferredClose { View* view; };

} // namespace

ViewManager::ViewManager(ServiceContext& service_context)
    : service_context(service_context)
    , event_receiver(service_context.make_event_receiver())
    , renderer(std::make_unique<ImGuiViewRenderer>()) {
    event_receiver.listen([this](OpenCommand const& command) {
        open_view(command.name, command.completion);
    });
    event_receiver.listen([this](CloseAllCommand) { close_all(); });
}

ViewManager::~ViewManager() = default;

std::uint64_t ViewManager::new_instance_id(ViewClass const* type) const {
    auto map_itr = m_views.find(type);
    if (map_itr == m_views.end()) return 0;
    auto& map = map_itr->second;
    std::uint64_t id = 0;
    for (;; ++id)
        if (!map.contains(id)) break;
    return id;
}

View* ViewManager::insert_view(ViewId view_id, std::unique_ptr<View> view) {
    assert(view != nullptr);
    auto* result = view.get();
    auto [_, success] =
        m_views[view_id.view_class].emplace(view_id.instance, std::move(view));
    assert(success);
    return result;
}

View* ViewManager::open_view(ViewRegistryEntry const& entry,
                             std::function<void(View&)> completion) {
    if (auto* existing = find_by_type(entry.view_class())) {
        existing->take_focus();
        if (completion) completion(*existing);
        return existing;
    }
    auto view = entry.create_view();
    auto view_id = ViewId{
        .view_class = &entry.view_class(),
        .instance = new_instance_id(&entry.view_class()),
    };
    view->initialize(view_id, service_context);
    auto* result = insert_view(view_id, std::move(view));
    result->take_focus();
    return result;
}

View* ViewManager::open_view(std::string_view name,
                             std::function<void(View&)> completion) {
    auto entry = ViewRegistry::find(name);
    if (!entry) {
        Logger::error("View does not exist: ", name);
        return nullptr;
    }
    return open_view(*entry, std::move(completion));
}

View const* ViewManager::find_by_type(ViewClass const& view_class) const {
    auto itr = m_views.find(&view_class);
    if (itr == m_views.end()) return nullptr;
    auto& map = itr->second;
    if (map.empty()) return nullptr;
    return map.begin()->second.get();
}

View const* ViewManager::find_by_id(ViewId view_id) const {
    auto map_itr = m_views.find(view_id.view_class);
    if (map_itr == m_views.end()) return nullptr;
    auto& map = map_itr->second;
    auto itr = map.find(view_id.instance);
    if (itr == map.end()) return nullptr;
    return itr->second.get();
}

void ViewManager::close_all() {
    for (auto& [view_class, map] : m_views)
        for (auto& [id, view] : map)
            view->request_close();
}

void ViewManager::process_deferred_operations() {
    // Destroy all views that requested to close
    for (auto& [view_class, map] : m_views)
        std::erase_if(map, [](auto& p) { return p.second->close_requested(); });
}

void ViewManager::display() {
    process_deferred_operations();
    renderer->begin_frame();
    m_focused_view = nullptr;
    for (auto& [view_class, map] : m_views) {
        for (auto& [instance, view] : map) {
            auto result = renderer->render_view(*view);
            if (result.focused) m_focused_view = view.get();
        }
    }
    renderer->end_frame();
    process_deferred_operations();
}

#ifndef NDEBUG
void ViewManager::draw_debugger() {
    if (m_focused_view)
        ImGui::Text("Focused View: [%u] %s",
                    (unsigned)m_focused_view->view_id().instance,
                    m_focused_view->title().c_str());
    else ImGui::Text("Focused View: none");
    ImGui::SeparatorText("Views");
    for (auto& [view_class, map] : m_views) {
        if (!ImGui::TreeNodeEx(view_class->name.c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen))
            continue;
        for (auto& [instance, view] : map) {
            if (!view) {
                ImGui::BulletText("[%u] NULL", (unsigned)instance);
                continue;
            }
            ImGui::PushID(static_cast<int>(instance));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("[%u] %s", (unsigned)instance, view->title().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%p)", static_cast<void const*>(view.get()));
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}
#endif

bool ViewManager::on_input(InputEvent& event) {
    /*
     * I would keep input routing here rather than in View.
     *
     * The manager knows which View is focused.
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

nlohmann::json ViewManager::save_settings() const {
    auto root = nlohmann::json::array();
    for (auto& [view_class, map] : m_views) {
        for (auto& [instance, view] : map) {
            auto object = nlohmann::json::object();
            object[config_key::type] = std::string(view->type_name());
            object[config_key::instance] = view->view_id().instance;
            object[config_key::hidden] = view->hidden();
            object[config_key::maximized] = view->maximized();
            object[config_key::state] = view->save_settings();
            root.push_back(std::move(object));
        }
    }
    return root;
}

void ViewManager::load_settings(nlohmann::json const& root) {
    m_views.clear();
    m_focused_view = nullptr;
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
        auto entry = ViewRegistry::find(*type_name);
        if (!entry) {
            Logger::error("Failed to deserialize view: unknown type: ",
                          *type_name);
            continue;
        }
        auto* view_class = &entry->view_class();
        auto view = entry->create_view();
        ViewId const view_id{
            .view_class = view_class,
            .instance = *instance,
        };
        view->initialize(view_id, service_context);
        if (auto hidden = json_find<bool>(node, config_key::hidden))
            view->set_hidden(*hidden);
        if (auto maximized = json_find<bool>(node, config_key::maximized))
            view->set_maximized(*maximized);
        if (auto* state = json_find(node, config_key::state))
            view->load_settings(*state);
        if (find_by_id(view_id) != nullptr) {
            Logger::error(
                "Failed to deserialize view: instance id already taken");
        } else {
            insert_view(view_id, std::move(view));
        }
    }
}