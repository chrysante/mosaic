#include "service/panel_manager.h"

#include <imgui.h>

#include "application/input_event.h"
#include "gui/panel.h"
#include "gui/panel_renderer.h"

using namespace mosaic;

namespace {
struct DeferredClose { Panel* panel; };
} // namespace

PanelManager::PanelManager()
    : renderer(std::make_unique<PanelRenderer>()) {}

PanelManager::~PanelManager() = default;

void PanelManager::process_deferred_operations() {
    std::erase_if(m_panels, [](auto& p) { return p->close_requested(); });
}

void PanelManager::display() {
    process_deferred_operations();
    renderer->begin_frame();
    m_focused_panel = nullptr;
    for (auto& panel : m_panels) {
        auto result = renderer->render_panel(*panel);
        if (result.focused) m_focused_panel = panel.get();
    }
    renderer->end_frame();
    process_deferred_operations();
}

#ifndef NDEBUG
void PanelManager::draw_debugger() {
    if (m_focused_panel)
        ImGui::Text("Focused Panel: %s", m_focused_panel->title().c_str());
    else ImGui::Text("Focused Panel: none");
    ImGui::SeparatorText("Views");
    for (int id_index = 0; auto& panel : m_panels) {
        if (!panel) {
            ImGui::BulletText("NULL");
            continue;
        }
        ImGui::PushID(static_cast<int>(id_index++));
        ImGui::BulletText("%s", panel->title().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%p)", static_cast<void const*>(panel.get()));
        ImGui::PopID();
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

PanelHandle PanelManager::create_panel() {
    m_panels.push_back(Panel::create_panel());
    return PanelHandle(m_panels.back().get());
}
