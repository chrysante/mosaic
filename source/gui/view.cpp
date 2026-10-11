#include "gui/view.h"

#include <imgui.h>

#include "gui/view_manager.h"
#include "service/service_context.h"

using namespace mosaic;
using namespace vml::short_types;

View::View(ViewDescription desc)
    : m_padding{-1, -1} {
    m_layout.size = desc.default_size;
    m_layout.min_size = desc.min_size;
}

void View::initialize(ViewId id, ServiceContext& services) {
    m_id = std::move(id);
    m_services = &services;

    m_event_emitter = services.make_event_emitter();
    m_event_receiver = services.make_event_receiver();

    if (m_title.empty()) m_title = m_id.type;

    on_create();
}

void View::destroy() { on_destroy(); }

void View::maximize() { m_layout.maximized = true; }

void View::restore() { m_layout.maximized = false; }

void View::toggle_maximize() { m_layout.maximized = !m_layout.maximized; }

vml::float2 View::padding() const {
    auto const& style = ImGui::GetStyle();
    return {m_padding.x >= 0 ? m_padding.x : style.WindowPadding.x,
            m_padding.y >= 0 ? m_padding.y : style.WindowPadding.y};
}

vml::float2 View::screen_space_to_view_space(vml::float2 p) const {
    return p - m_layout.position;
}

vml::float2 View::view_space_to_screen_space(vml::float2 p) const {
    return p + m_layout.position;
}