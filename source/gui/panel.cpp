#include "gui/panel.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "gui/panel_manager.h"
#include "imgui_internal.h"
#include "service/service_context.h"

using namespace mosaic;
using namespace vml::short_types;

void Panel::initialize(PanelId panel_id, ServiceContext& services) {
    m_panel_id = panel_id;
    m_services = &services;

    m_event_emitter = services.make_event_emitter();
    m_event_receiver = services.make_event_receiver();

    if (m_title.empty()) m_title = panel_id.panel_class->name;

    on_create();
}

void Panel::destroy() { on_destroy(); }

vml::float2 Panel::padding() const {
    auto const& style = ImGui::GetStyle();
    return {m_padding.x >= 0 ? m_padding.x : style.WindowPadding.x,
            m_padding.y >= 0 ? m_padding.y : style.WindowPadding.y};
}

vml::float2 Panel::screen_space_to_view_space(vml::float2 p) const {
    auto* window = ImGui::GetCurrentWindowRead();
    return p - (vml::float2)window->Pos;
}

vml::float2 Panel::view_space_to_screen_space(vml::float2 p) const {
    auto* window = ImGui::GetCurrentWindowRead();
    return p + (vml::float2)window->Pos;
}

nlohmann::json Panel::save_settings() const { return nlohmann::json::object(); }