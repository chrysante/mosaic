#pragma once

#include <any>
#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>
#include <vml/vml.hpp>

#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/gui/view_id.h>
#include <mosaic/service/event_emitter.h>
#include <mosaic/service/event_receiver.h>

namespace mosaic {

class ServiceContext;
class ViewManager;
class ViewRegistryEntry;
class ImGuiViewRenderer;
struct InputEvent;

class View {
public:
    virtual ~View() = default;

    View(View const&) = delete;
    View& operator=(View const&) = delete;

    ViewClass const& view_class() const { return *view_id().view_class; }

    ViewId const& view_id() const { return m_view_id; }

    std::string const& type_name() const { return view_class().name; }

    /// User-visible title.
    std::string const& title() const { return m_title; }

    void set_title(std::string title) { m_title = std::move(title); }

    bool hidden() const { return m_hidden; }
    void set_hidden(bool value = true) { m_hidden = value; }
    void toggle_hidden() { set_hidden(!hidden()); }

    bool maximized() const { return m_maximized; }
    /// These are semantic operations. ViewManager/renderer performs the
    /// actual platform/ImGui work.
    void set_maximized(bool value) { m_maximized = value; }
    void maximize() { set_maximized(true); }
    void restore() { set_maximized(false); }
    void toggle_maximized() { set_maximized(!maximized()); }

    bool close_requested() const { return m_close_requested; }
    void request_close() { m_close_requested = true; }

    bool focused() const { return m_runtime.focused; }
    void take_focus() { m_focus_requested = true; }

    /// Padding used by the ImGui renderer.
    vml::float2 padding() const;

    /// Use a negative value for the default value
    void set_padding(vml::float2 padding) { m_padding = padding; }

    void set_bg_color(std::optional<vml::float4> color) { m_bg_color = color; }

    std::optional<vml::float4> const& bg_color() const { return m_bg_color; }

    CommandPalette const& command_palette() const { return m_command_palette; }

    /// Converts between the screen-space and view-space coordinate systems.
    vml::float2 screen_space_to_view_space(vml::float2 p) const;
    vml::float2 view_space_to_screen_space(vml::float2 p) const;

    void send_data(std::any data) { receive_data(std::move(data)); }

    Toolbar const& toolbar() const { return m_toolbar; }

    bool uses_toolbar() const { return m_uses_toolbar; }

protected:
    View() = default;

    CommandPalette& command_palette_write() { return m_command_palette; }

    void set_toolbar(Toolbar toolbar) {
        m_toolbar = std::move(toolbar);
        m_uses_toolbar = true;
    }

    void remove_toolbar() {
        m_toolbar = {};
        m_uses_toolbar = false;
    }

    void override_toolbar_style(ToolbarStyle style) {
        m_toolbar_style = std::make_unique<ToolbarStyle>(style);
    }

    void reset_toolbar_style() { m_toolbar_style.reset(); }

private:
    friend class ViewManager;
    friend class ViewRegistryEntry;
    friend class ImGuiViewRenderer;

    struct RuntimeState {
        bool focused = false;
        bool hovered = false;
    };

    void initialize(ViewId view_id, ServiceContext& services);
    void destroy();

    void set_focused_from_renderer(bool value) { m_runtime.focused = value; }

    /// Called once after construction and framework initialization.
    virtual void on_create() {}
    /// Called once before destruction.
    virtual void on_destroy() {}
    /// Called every frame while the view is visible.
    virtual void display() = 0;

    virtual nlohmann::json save_settings() const;
    virtual void load_settings(nlohmann::json const&) {}

    virtual void receive_data(std::any) {}

    ViewId m_view_id;
    std::string m_title;
    bool m_hidden = false;
    bool m_maximized = false;
    vml::float2 m_padding = {-1.0f, -1.0f};
    std::optional<vml::float4> m_bg_color;

    bool m_close_requested = false;
    bool m_focus_requested = false;

    ServiceContext* m_services = nullptr;
    EventReceiver m_event_receiver;
    EventEmitter m_event_emitter;
    CommandPalette m_command_palette;
    Toolbar m_toolbar;
    std::unique_ptr<ToolbarStyle> m_toolbar_style;
    bool m_uses_toolbar = false;
    RuntimeState m_runtime;
};

} // namespace mosaic