#pragma once

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <utl/messenger.hpp>
#include <vml/vml.hpp>
#include <yaml-cpp/yaml.h>

#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/service/event_emitter.h>
#include <mosaic/service/event_receiver.h>

namespace mosaic {

class ServiceContext;
class ViewManager;
class ViewRegistryEntry;
class ImGuiViewRenderer;
struct InputEvent;

struct ViewId {
    std::string type;
    uint64_t instance = 0;

    friend bool operator==(ViewId const&, ViewId const&) = default;
};

struct ViewDescription {
    vml::int2 default_size = {500, 400};
    vml::int2 min_size = {300, 200};
};

struct ViewLayout {
    vml::float2 position = {};
    vml::float2 size = {};
    vml::float2 min_size = {};

    bool hidden = false;
    bool maximized = false;
};

class View {
public:
    virtual ~View() = default;

    View(View const&) = delete;
    View& operator=(View const&) = delete;

    ViewId const& id() const { return m_id; }

    /// Stable type name, e.g. "scene", "console", "inspector".
    std::string const& type_name() const { return m_id.type; }

    /// User-visible title.
    std::string const& title() const { return m_title; }

    void set_title(std::string title) { m_title = std::move(title); }

    ViewLayout const& layout() const { return m_layout; }
    ViewLayout& layout() { return m_layout; }

    bool hidden() const { return m_layout.hidden; }
    void set_hidden(bool value = true) { m_layout.hidden = value; }
    void toggle_hidden() { m_layout.hidden = !m_layout.hidden; }

    bool maximized() const { return m_layout.maximized; }

    bool close_requested() const { return m_close_requested; }
    void request_close() { m_close_requested = true; }

    /// These are semantic operations. ViewManager/renderer performs the
    /// actual platform/ImGui work.
    void maximize();
    void restore();
    void toggle_maximize();

    bool focused() const { return m_runtime.focused; }

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
    explicit View(ViewDescription desc = {});

    CommandPalette& command_palette_write() { return m_command_palette; }

    void set_toolbar(Toolbar toolbar) {
        m_toolbar = std::move(toolbar);
        m_uses_toolbar = true;
    }

    void remove_toolbar() {
        m_toolbar = {};
        m_uses_toolbar = false;
    }

    /// Called once after construction and framework initialization.
    virtual void on_create() {}

    /// Called once before destruction.
    virtual void on_destroy() {}

    /// Called every frame while the view is visible.
    virtual void display() = 0;

    /// Derived state only. Framework state is serialized by ViewManager.
    virtual YAML::Node serialize_state() const { return {}; }

    virtual void deserialize_state(YAML::Node) {}

    virtual void receive_data(std::any) {}

private:
    friend class ViewManager;
    friend class ViewRegistryEntry;
    friend class ImGuiViewRenderer;

    struct RuntimeState {
        bool focused = false;
        bool hovered = false;
    };

    void initialize(ViewId id, ServiceContext& services);
    void destroy();

    void set_focused_from_renderer(bool value) { m_runtime.focused = value; }

    ViewId m_id;
    std::string m_title;
    ViewLayout m_layout;

    vml::float2 m_padding;
    std::optional<vml::float4> m_bg_color;

    bool m_close_requested = false;

    ServiceContext* m_services = nullptr;
    EventReceiver m_event_receiver;
    EventEmitter m_event_emitter;
    CommandPalette m_command_palette;
    Toolbar m_toolbar;
    bool m_uses_toolbar = false;
    RuntimeState m_runtime;
};

} // namespace mosaic