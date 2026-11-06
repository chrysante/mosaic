#include "gui/panel.h"

#include "gui/view.h"

using namespace mosaic;

Panel::Panel() = default;

std::unique_ptr<Panel> Panel::create_panel() {
    return std::unique_ptr<Panel>(new Panel());
}

Panel::~Panel() = default;

void Panel::set_toolbar(Owner<ui::View> toolbar) {
    m_toolbar = std::move(toolbar);
}

void Panel::set_view(Owner<ui::View> view) { m_view = std::move(view); }