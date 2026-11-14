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

struct PanelSizeConstraints {
    vml::float2 min_size = {300.0f, 300.0f};
    vml::float2 max_size = {FLT_MAX, FLT_MAX};
};

void to_json(nlohmann::json&, PanelSizeConstraints const&);
void from_json(nlohmann::json const&, PanelSizeConstraints&);

struct PanelGeometry {
    Rect frame = {.origin = {500.0f, 200.0f}, .size = {800.0f, 600.0f}};
    PanelSizeConstraints size_constraints;
};

void to_json(nlohmann::json&, PanelGeometry const&);
void from_json(nlohmann::json const&, PanelGeometry&);

enum class PanelToolbarHeight { small, medium, large };

struct PanelStyle {
    PanelToolbarHeight toolbar_height = PanelToolbarHeight::medium;
};

struct PanelDesc {
    PanelGeometry geometry;
    std::string title;
    PanelStyle style;
};

class PanelDelegate {
public:
    virtual ~PanelDelegate() = default;
    virtual void on_destroy() {}
    virtual bool on_close_request() { return true; }
};

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

    /// Request the panel to close. Delegate is asked for confirmation
    void request_close();

    /// Command the panel to close. Delegate is not asked for confirmation
    void command_close();

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

    PanelStyle const& style() const { return m_style; }

    void set_style(PanelStyle new_style) { m_style = new_style; }

private:
    friend class PanelManager;
    friend class PanelRenderer;

    explicit Panel(int id, PanelDesc const& desc,
                   std::unique_ptr<PanelDelegate> delegate);
    static std::unique_ptr<Panel>
    create_panel(int id, PanelDesc const& desc,
                 std::unique_ptr<PanelDelegate> delegate);

    int m_id;
    PanelGeometry m_geometry;
    std::string m_title;
    PanelStyle m_style;
    bool m_hidden = false;
    bool m_maximized = false;
    bool m_close_requested = false;
    bool m_focus_requested = false;
    bool m_focused = false;
    bool m_hovered = false;
    Owner<ui::View> m_toolbar;
    Owner<ui::View> m_view;
    std::shared_ptr<Handle<Panel>::ControlBlock> m_control_block;
    std::unique_ptr<PanelDelegate> m_delegate;
};

} // namespace mosaic
