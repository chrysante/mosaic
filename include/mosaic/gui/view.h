#pragma once

#include <optional>
#include <string>

#include <utl/hashtable.hpp>
#include <utl/messenger.hpp>
#include <vml/vml.hpp>
#include <yaml-cpp/yaml.h>

#include <mosaic/gui/command_palette.h>
#include <mosaic/gui/toolbar.h>
#include <mosaic/service/event_emitter.h>
#include <mosaic/service/event_receiver.h>

namespace mosaic {

class InputEvent;
class ServiceContext;
class Window;

/// View constructor arguments
struct ViewDescription {
    /// The default display size of the view
    vml::int2 default_size = {500, 400};

    /// The minimum display size of the view
    vml::int2 min_size = {300, 200};
};

/// Base class of all UI views
///
/// Wraps an ImGui window and extends and specializes its functionality
class View {
public:
    virtual ~View() = default;

    /// \returns `true` if this view is the focused view
    bool focused() const;

    /// \returns `true` if this view is maximized
    bool maximized() const { return m_data.maximized; }

    /// \returns `true` if the user requested to close this view
    bool should_close() const { return !m_data.open; }

    /// \returns the size of this view's window.
    vml::int2 size() const { return m_data.size; }

    /// \returns the position of this view in screen coordinates
    vml::int2 position() const { return m_data.position; }

    /// \returns the padding of this view
    vml::float2 padding() const;

    /// \returns the title of this view
    std::string const& title() const { return m_data.title; }

    /// \returns the static name of this view
    std::string const& static_name() const { return m_data.static_name; }

    /// \returns the window in which this view resides
    Window& window() const;

    /// Focuses this view
    void set_focused();

    /// Maximizes this view
    void maximize();

    /// Restores this view
    void restore();

    /// Maximizes the view if restores and vice versa
    void toggle_maximize();

    /// Sets the window title to \p title
    void set_title(std::string title) { m_data.title = std::move(title); }

    /// Sets the window padding to \p padding
    /// If \p padding is unengaged `ImGui::Style().WindowPadding` will be used
    void set_padding(vml::vector<std::optional<float>, 2> padding) {
        m_data.padding = padding;
    }

    /// Sets the horizontal window padding
    void set_padding_x(std::optional<float> value) { m_data.padding.x = value; }

    /// Sets the vertical window padding
    void set_padding_y(std::optional<float> value) { m_data.padding.y = value; }

    /// Sets the window background color to \p color
    /// If \p color is unengaged `ImGuiCol_WindowBG` will be used
    void set_bg_color(std::optional<vml::float4> color) {
        m_data.bg_color = color;
    }

    /// \returns the attached command palette
    CommandPalette const& command_palette() const { return m_command_palette; }

    /// Converts the point \p p from screen coordinates into view coordinates
    vml::float2 screen_space_to_view_space(vml::float2 p) const;

    /// Converts the point \p p from view coordinates into screen coordinates
    vml::float2 view_space_to_screen_space(vml::float2 p) const;

    /// Can be used to send data to derived views without knowing their type.
    /// Unexpected data will be ignored
    void send_data(std::any data) { receive_data(std::move(data)); }

    /// \returns `true` if this view is hidden.
    /// Hidden views don't have their `display()` function called on updates
    bool is_hidden() const { return m_data.hidden; }

    /// Hides or (unhides) this view
    void set_hidden(bool value = true) { m_data.hidden = value; }

    /// Hides or (unhides) this view
    void toggle_hidden() { set_hidden(m_data.hidden ^ true); }

    struct Impl;

protected:
    explicit View(ViewDescription desc = {});

    /// \returns a mutable reference to the attached command palette
    CommandPalette& command_palette_write() { return m_command_palette; }

    /// Calls `display()` on the toolbar of this view. This is virtual to allow
    /// customized toolbar rendering
    virtual void display_toolbar();

    /// \returns a pointer this view's toolbar. This will be null if no toolbar
    /// is set
    Toolbar const* toolbar() const { return m_toolbar ? &*m_toolbar : nullptr; }

    /// Sets the toolbar of this view
    void set_toolbar(Toolbar toolbar) { m_toolbar = std::move(toolbar); }

private:
    /// Called after the view has been created and potentially deserialized
    virtual void init() {};

    /// Called on every frame the application is rendered
    virtual void display() {};

    /// Callback to serialize derived view data
    virtual YAML::Node serialize() const { return YAML::Node{}; }

    /// Callback to deserialize derived view data
    virtual void deserialize(YAML::Node) {}

    /// Callback to receive data
    virtual void receive_data(std::any){};

    friend class ViewRegistryEntry;
    friend class ViewManager;

    void do_init();
    void do_frame();
    void do_on_input(InputEvent& event);
    YAML::Node do_serialize() const;
    static std::unique_ptr<View>
    do_deserialize(YAML::Node const& node, ServiceContext& service_context);

    struct Data {
        /// This is set by `do_init()` to a new view ID if it is -1 and by
        /// `do_deserialize()` if the view is not newly created
        int ID = -1;
        /// Name of the derived view class. Used for serialization
        std::string static_name;

        vml::float2 position;
        vml::float2 size;
        vml::float2 min_size;
        bool docked = false;
        std::string title;
        std::string title_id;
        void* imgui_window = nullptr;
        ServiceContext* service_context = nullptr;
        Window* window = nullptr;
        vml::vector<std::optional<float>, 2> padding;
        std::optional<vml::float4> bg_color;
        bool maximized = false;
        bool open = true;
        bool is_focused = false;
        bool hidden = false;
        /// `ImGuiWindowFlags` to use on the next frame. This is reset after the
        /// call to `Begin()`
        int next_frame_flags = 0;
    };
    struct InputState {
        bool hovered = false;
        bool capture = false;
    };
    Data m_data;
    EventReceiver m_event_receiver;
    EventEmitter m_event_emitter;
    InputState m_input_state;
    CommandPalette m_command_palette;
    std::optional<Toolbar> m_toolbar;
};

} // namespace mosaic
