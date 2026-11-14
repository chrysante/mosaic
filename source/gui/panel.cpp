#include "gui/panel.h"

#include <nlohmann/json.hpp>

#include "gui/view.h"
#include "support/json_support.h"

using namespace mosaic;

void mosaic::to_json(nlohmann::json& j, PanelSizeConstraints const& v) {
    j["min_size"] = v.min_size;
    j["max_size"] = v.max_size;
}

void mosaic::from_json(nlohmann::json const& j, PanelSizeConstraints& v) {
    j.at("min_size").get_to(v.min_size);
    j.at("max_size").get_to(v.max_size);
}

void mosaic::to_json(nlohmann::json& j, PanelGeometry const& v) {
    j["frame"] = v.frame;
    j["size_constraints"] = v.size_constraints;
}

void mosaic::from_json(nlohmann::json const& j, PanelGeometry& v) {
    v.size_constraints = json_find(j, "size_constraints", v.size_constraints);
    v.frame = json_find(j, "frame", v.frame);
}

void Panel::request_close() {
    if (m_delegate) m_close_requested = m_delegate->on_close_request();
    else m_close_requested = true;
}

void Panel::command_close() { m_close_requested = true; }

Panel::Panel(int id, PanelDesc const& desc,
             std::unique_ptr<PanelDelegate> delegate)
    : m_id(id)
    , m_geometry(desc.geometry)
    , m_title(desc.title)
    , m_style(desc.style)
    , m_delegate(std::move(delegate)) {}

std::unique_ptr<Panel>
Panel::create_panel(int id, PanelDesc const& desc,
                    std::unique_ptr<PanelDelegate> delegate) {
    return std::unique_ptr<Panel>(new Panel(id, desc, std::move(delegate)));
}

Panel::~Panel() {
    if (m_delegate) m_delegate->on_destroy();
    if (m_control_block) m_control_block->ptr = nullptr;
}

void Panel::set_toolbar(Owner<ui::View> toolbar) {
    m_toolbar = std::move(toolbar);
}

void Panel::set_view(Owner<ui::View> view) { m_view = std::move(view); }
