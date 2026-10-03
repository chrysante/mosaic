#include "gui/view_manager.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>
#include <vml/vml.hpp>

#include "application/application.h"
#include "application/input_event.h"
#include "application/window.h"
#include "gui/view.h"
#include "gui/view_registry.h"
#include "service/service_context.h"
#include "support/logger.h"

using namespace mosaic;
using namespace vml::short_types;

struct ViewManager::Impl {
    static void display_debug_info(View const& view) {
        auto* im_window = (ImGuiWindow*)view.m_data.imgui_window;
        auto* window = view.m_data.window;
        if (!im_window || !window) return;
        ImGui::Begin(view.m_data.title_id.c_str());
        ImGui::SetCursorPos({});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
        ImGui::BeginChild("Debug Overlay", {}, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoInputs);
        ImGui::PopStyleColor();
        auto* DL = ImGui::GetWindowDrawList();
        float2 pos =
            (float2)im_window->Pos + (float2)ImGui::GetStyle().WindowPadding;
        pos.y += window->titlebar_height();
        ImU32 color = IM_COL32(255, 0, 255, 255);
        auto field = [&](std::string_view label, auto const&... args) {
            DL->AddText(pos, color, utl::strcat(label, ": ", args...).c_str());
            pos.y += ImGui::GetFontSize();
        };
        field("System window position: ", window->position());
        field("System window size: ", window->size());
        field("ImGui window position: ", (float2)im_window->Pos);
        field("ImGui window size: ", (float2)im_window->Size);
        field("Title: ", view.m_data.title);
        field("Docked: ", view.m_data.docked ? "YES" : "NO");
        field("Padding X: ", view.m_data.padding.x
                                 ? utl::strcat(*view.m_data.padding.x)
                                 : "Default");
        field("Padding X: ", view.m_data.padding.y
                                 ? utl::strcat(*view.m_data.padding.y)
                                 : "Default");
        field("Is focused: ", view.m_data.is_focused ? "YES" : "NO");
        ImGui::EndChild();
        ImGui::End();
    }
};

ViewManager::ViewManager(ServiceContext& service_context)
    : event_receiver(service_context.make_event_receiver())
    , service_context(service_context) {
    event_receiver.listen([this](OpenCommand const& cmd) {
        auto* view = open_view(cmd.name, cmd.completion);
        if (cmd.result) *cmd.result = view;
    });
    event_receiver.listen([this](CloseAllCommand) { close_all(); });
    event_receiver.listen(
        [this](ToggleDebugView) { display_debugger ^= true; });
}

ViewManager::~ViewManager() = default;

View* ViewManager::open_view(ViewRegistryEntry const& reg_entry) {
    views.push_back(reg_entry.create_view(service_context));
    auto* view = views.back().get();
    view->m_data.position = {250, 250};
    view->do_init();
    return view;
}

View* ViewManager::open_view(std::string_view name,
                             std::function<void(View&)> completion) {
    auto* view = [&]() -> View* {
        if (auto* view = find_by_static_name(name)) {
            view->set_focused();
            return view;
        }
        auto reg_entry = ViewRegistry::get(name);
        if (!reg_entry) {
            Logger::error("View does not exist: ", name);
            return nullptr;
        }
        return open_view(*reg_entry);
    }();
    if (view && completion) completion(*view);
    return view;
}

View* ViewManager::find_by_static_name(std::string_view name) {
    auto itr = std::ranges::find_if(
        views, [&](auto& view) { return view->static_name() == name; });
    return itr != views.end() ? itr->get() : nullptr;
}

void ViewManager::close_focused_view() {
    if (auto* view = focused_view()) {
        /// Will be closed by the next call to `display()`
        view->m_data.open = false;
    }
}

void ViewManager::close_all() { views.clear(); }

void ViewManager::display() {
    /// Clear closing views
    views.erase(std::remove_if(views.begin(), views.end(),
                               [](auto& view) { return !view->m_data.open; }),
                views.end());
    /// If we have a maximized view we only display that. This is wrong because
    /// for multiple displays it would hide the views on other displays but we
    /// leave it for now
    auto max =
        std::ranges::find_if(views, [](auto&& v) { return v->maximized(); });
    if (max != views.end()) {
        auto& maximized_view = **max;
        maximized_view.do_frame();
        return;
    }
    for (auto& view : views) {
        if (view->is_hidden()) continue;
        view->do_frame();
        if (display_debugger) Impl::display_debug_info(*view);
    }
}

bool ViewManager::on_input(InputEvent& event) {
#if 0
    /// Give to focused view
    if (auto* focused = focused_view()) {
        focused->do_on_input(event);
        if (event.handled()) return true;
    }
    /// Give to all unfocused views
    for (auto& view : views) {
        if (view->focused()) continue;
        view->do_on_input(event);
        if (event.handled()) return true;
    }
#endif
    return false;
}

YAML::Node ViewManager::serialize() const {
    YAML::Node root;
    for (auto& view : views)
        root.push_back(view->do_serialize());
    return root;
}

void ViewManager::deserialize(YAML::Node const& node) {
    views.clear();
    for (auto view_data : node) {
        auto owner = View::do_deserialize(view_data, service_context);
        if (!owner) continue;
        auto* view = owner.get();
        views.push_back(std::move(owner));
        view->do_init();
    }
}

View const* ViewManager::focused_view() const {
    auto itr =
        std::ranges::find_if(views, [](auto& v) { return v->focused(); });
    return itr != views.end() ? itr->get() : nullptr;
}
