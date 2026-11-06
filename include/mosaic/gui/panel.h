#pragma once

#include <cfloat>
#include <string>

#include <nlohmann/json_fwd.hpp>
#include <vml/vml.hpp>

#include <mosaic/support/geometry.h>
#include <mosaic/support/memory.h>

namespace mosaic {

class PanelManager;
class PanelRenderer;
namespace ui { class View; }
namespace internal { struct PanelDeleter; }

class PanelHandle;

struct PanelSizeConstraints {
    vml::float2 min_size = {300.0f, 300.0f};
    vml::float2 max_size = {FLT_MAX, FLT_MAX};
};

void to_json(nlohmann::json&, PanelSizeConstraints const&);
void from_json(nlohmann::json const&, PanelSizeConstraints&);

struct PanelGeometry {
    Rect frame = {.origin = {500.0f, 500.0f}, .size = {800.0f, 600.0f}};
    PanelSizeConstraints size_constraints;
};

void to_json(nlohmann::json&, PanelGeometry const&);
void from_json(nlohmann::json const&, PanelGeometry&);

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

    PanelSizeConstraints const& size_constraints() const {
        return m_geometry.size_constraints;
    }

    PanelGeometry const& geometry() const { return m_geometry; }

private:
    friend class PanelManager;
    friend class PanelRenderer;
    friend struct mosaic::internal::PanelDeleter;

    explicit Panel(PanelGeometry initial_geometry);
    static std::unique_ptr<Panel> create_panel(PanelGeometry initial_geometry);

    PanelGeometry m_geometry;
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
