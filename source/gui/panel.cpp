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

Panel::Panel(PanelGeometry initial_geometry)
    : m_geometry(initial_geometry) {}

std::unique_ptr<Panel> Panel::create_panel(PanelGeometry initial_geometry) {
    return std::unique_ptr<Panel>(new Panel(initial_geometry));
}

Panel::~Panel() = default;

void Panel::set_toolbar(Owner<ui::View> toolbar) {
    m_toolbar = std::move(toolbar);
}

void Panel::set_view(Owner<ui::View> view) { m_view = std::move(view); }
