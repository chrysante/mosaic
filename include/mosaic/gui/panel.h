#pragma once

#include <string>

#include <mosaic/support/memory.h>

namespace mosaic {

class PanelManager;
class PanelRenderer;
namespace ui { class View; }
namespace internal { struct PanelDeleter; }

class PanelHandle;

class Panel final {
public:
    ~Panel();

    Panel(Panel const&) = delete;
    Panel& operator=(Panel const&) = delete;

    /// User-visible title.
    std::string const& title() const { return m_title; }
    void set_title(std::string title) { m_title = std::move(title); }

    bool hidden() const { return m_hidden; }
    void set_hidden(bool value = true) { m_hidden = value; }
    void toggle_hidden() { set_hidden(!hidden()); }

    bool maximized() const { return m_maximized; }
    void set_maximized(bool value) { m_maximized = value; }
    void maximize() { set_maximized(true); }
    void restore() { set_maximized(false); }
    void toggle_maximized() { set_maximized(!maximized()); }

    bool close_requested() const { return m_close_requested; }
    void request_close() { m_close_requested = true; }

    bool focused() const { return m_focused; }
    void take_focus() { m_focus_requested = true; }

    ui::View* view() const { return m_view.get(); }
    ui::View* toolbar() const { return m_toolbar.get(); }

    void set_toolbar(Owner<ui::View> toolbar);

    void set_view(Owner<ui::View> view);

private:
    friend class PanelManager;
    friend class PanelRenderer;
    friend struct mosaic::internal::PanelDeleter;

    Panel();
    static std::unique_ptr<Panel> create_panel();

    std::string m_title;
    bool m_hidden = false;
    bool m_maximized = false;
    bool m_close_requested = false;
    bool m_focus_requested = false;
    bool m_focused = false;
    bool m_hovered = false;
    Owner<ui::View> m_toolbar;
    Owner<ui::View> m_view;
};

} // namespace mosaic

struct mosaic::internal::PanelDeleter {
    void operator()(Panel* panel) const { panel->m_close_requested = true; }
};

class mosaic::PanelHandle
    : public std::unique_ptr<Panel, mosaic::internal::PanelDeleter> {
public:
    using unique_ptr::unique_ptr;
};