#pragma once

#include <memory>
#include <vector>

#include <mosaic/support/memory.h>

namespace mosaic {

struct InputEvent;
class Panel;
class PanelRenderer;
class PanelDelegate;
struct PanelDesc;

class PanelManager {
public:
    PanelManager();
    ~PanelManager();

    PanelManager(PanelManager const&) = delete;
    PanelManager& operator=(PanelManager const&) = delete;

    void display();

    bool on_input(InputEvent& event);

    Panel* focused_panel() { return m_focused_panel; }
    Panel const* focused_panel() const { return m_focused_panel; }

    Handle<Panel> create_panel(PanelDesc const& desc,
                               std::unique_ptr<PanelDelegate> delegate);

#ifndef NDEBUG
    void draw_debugger();
#endif

private:
    void process_deferred_operations();

    std::unique_ptr<PanelRenderer> renderer;
    std::vector<std::unique_ptr<Panel>> m_panels;
    Panel* m_focused_panel = nullptr;
};

} // namespace mosaic
