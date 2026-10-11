#include "gui/view_manager.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "application/debug_settings.h"
#include "application/input_event.h"
#include "gui/view.h"
#include "gui/view_registry.h"
#include "gui/view_renderer.h"
#include "service/service_context.h"
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

ViewManager::~ViewManager() { views.clear(); }

uint64_t ViewManager::allocate_instance_id(std::string_view type) {
    auto& next = next_instance_ids[std::string(type)];
    return next++;
}

View* ViewManager::open_view(ViewRegistryEntry const& entry,
                             std::function<void(View&)> completion) {
    if (auto* existing = find_by_type(entry.name())) {
        focus(*existing);
        if (completion) completion(*existing);
        return existing;
    }
    auto view = entry.create_view(service_context);
    auto id = ViewId{
        .type = entry.name(),
        .instance = allocate_instance_id(entry.name()),
    };
    view->initialize(id, service_context);
    /*
     * Default position for newly created views.
     *
     * This is manager/layout policy rather than View construction.
     */
    view->layout().position = {250, 250};
    auto* result = view.get();
    views.push_back(std::move(view));
    focus(*result);
    return result;
}

View* ViewManager::open_view(std::string_view name,
                             std::function<void(View&)> completion) {
    auto entry = ViewRegistry::get(name);
    if (!entry) {
        Logger::error("View does not exist: ", name);
        return nullptr;
    }
    return open_view(*entry, std::move(completion));
}

View* ViewManager::find_by_id(ViewId const& id) {
    auto itr = std::ranges::find_if(
        views, [&](auto const& view) { return view->id() == id; });
    return itr != views.end() ? itr->get() : nullptr;
}

View const* ViewManager::find_by_id(ViewId const& id) const {
    auto itr = std::ranges::find_if(
        views, [&](auto const& view) { return view->id() == id; });
    return itr != views.end() ? itr->get() : nullptr;
}

View* ViewManager::find_by_type(std::string_view type) {
    auto itr = std::ranges::find_if(
        views, [&](auto const& view) { return view->type_name() == type; });
    return itr != views.end() ? itr->get() : nullptr;
}

View const* ViewManager::find_by_type(std::string_view type) const {
    auto itr = std::ranges::find_if(
        views, [&](auto const& view) { return view->type_name() == type; });
    return itr != views.end() ? itr->get() : nullptr;
}

void ViewManager::focus(View& view) {
    auto view_itr = std::ranges::find_if(
        views, [&](auto const& candidate) { return candidate.get() == &view; });
    if (view_itr == views.end()) return;
    focused = &view;

    /*
     * The actual ImGui focus is deliberately performed by the renderer.
     *
     * The renderer can add:
     *
     *     ImGui::FocusWindow(...)
     *
     * when the view has an existing ImGui window.
     */
}

void ViewManager::close(View& view) {
    view.request_close();
    if (focused == &view) focused = nullptr;
}

void ViewManager::close_focused_view() {
    if (focused) close(*focused);
}

void ViewManager::close_all() {
    for (auto& view : views)
        view->request_close();
    focused = nullptr;
}

void ViewManager::process_deferred_operations() {
    /*
     * No View is destroyed while display() is iterating.
     */
    std::erase_if(views,
                  [](auto const& view) { return view->close_requested(); });
    bool clear_focused =
        focused && std::ranges::none_of(views, [&](auto const& view) {
            return view.get() == focused;
        });
    if (clear_focused) focused = nullptr;
}

void ViewManager::display() {
    if (want_debug_overlays()) display_debugger();
    process_deferred_operations();
    renderer->begin_frame();
    for (auto& view : views)
        renderer->render_view(*view);

    renderer->end_frame();
    process_deferred_operations();
}

void ViewManager::display_debugger() {
    if (ImGui::Begin("View Manager Debugger")) {
        ImGui::Text("Views: %zu", views.size());
        ImGui::Separator();
        for (std::size_t i = 0; i < views.size(); ++i) {
            auto const* view = views[i].get();
            if (!view) {
                ImGui::BulletText("[%zu] NULL", i);
                continue;
            }
            ImGui::PushID(static_cast<int>(i));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text("[%zu] %s", i, view->title().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%p)", static_cast<void const*>(view));
            ImGui::PopID();
        }
    }
    ImGui::End();
}

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

YAML::Node ViewManager::serialize() const {
    YAML::Node root;
    for (auto const& view : views) {
        YAML::Node node;
        node["Type"] = std::string(view->type_name());
        node["Instance"] = view->id().instance;
        node["Layout"]["Position"] = view->layout().position;
        node["Layout"]["Size"] = view->layout().size;
        node["Layout"]["Hidden"] = view->hidden();
        node["Layout"]["Maximized"] = view->maximized();
        node["State"] = view->serialize_state();
        root.push_back(std::move(node));
    }

    return root;
}

void ViewManager::deserialize(YAML::Node const& root) {
    views.clear();
    focused = nullptr;
    if (!root || !root.IsSequence()) return;
    for (auto const& node : root) {
        try {
            auto type = node["Type"].as<std::string>();
            auto instance = node["Instance"].as<uint64_t>();
            auto entry = ViewRegistry::get(type);
            if (!entry) {
                Logger::error("Failed to deserialize view: unknown type: ",
                              type);
                continue;
            }
            auto view = entry->create_view(service_context);
            view->initialize(
                ViewId{
                    .type = type,
                    .instance = instance,
                },
                service_context);
            if (auto layout_node = node["Layout"]) {
                auto& layout = view->layout();
                layout.position =
                    layout_node["Position"].as<vml::float2>(layout.position);
                layout.size = layout_node["Size"].as<vml::float2>(layout.size);
                layout.hidden = layout_node["Hidden"].as<bool>(false);
                layout.maximized = layout_node["Maximized"].as<bool>(false);
            }
            view->deserialize_state(node["State"]);
            /*
             * Keep future newly-created IDs above restored IDs.
             */
            auto& next = next_instance_ids[type];
            next = std::max(next, instance + 1);
            views.push_back(std::move(view));
        } catch (std::exception const& e) {
            Logger::error("Failed to deserialize view: ", e.what());
        }
    }
}

View* ViewManager::focused_view() { return focused; }

View const* ViewManager::focused_view() const { return focused; }