#include "gui/view.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "gui/view_manager.h"
#include "imgui_internal.h"
#include "service/service_context.h"

using namespace mosaic;
using namespace vml::short_types;

void View::initialize(ViewId view_id, ServiceContext& services) {
    m_view_id = view_id;
    m_services = &services;

    m_event_emitter = services.make_event_emitter();
    m_event_receiver = services.make_event_receiver();

    if (m_title.empty()) m_title = view_id.view_class->name;

    on_create();
}

void View::destroy() { on_destroy(); }

vml::float2 View::padding() const {
    auto const& style = ImGui::GetStyle();
    return {m_padding.x >= 0 ? m_padding.x : style.WindowPadding.x,
            m_padding.y >= 0 ? m_padding.y : style.WindowPadding.y};
}

vml::float2 View::screen_space_to_view_space(vml::float2 p) const {
    auto* window = ImGui::GetCurrentWindowRead();
    return p - (vml::float2)window->Pos;
}

vml::float2 View::view_space_to_screen_space(vml::float2 p) const {
    auto* window = ImGui::GetCurrentWindowRead();
    return p + (vml::float2)window->Pos;
}

nlohmann::json View::save_settings() const { return nlohmann::json::object(); }