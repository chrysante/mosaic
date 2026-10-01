#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <mosaic/service/event_receiver.h>
#include <mosaic/support/serialization.h>

namespace mosaic {

class ServiceContext;
class View;
class ViewRegistryEntry;
class InputEvent;

/// Creates and destroys all `View` instances in the application
class ViewManager {
public:
    struct ToggleDebugView {};

    struct OpenCommand {
        std::string name;
        View** result = nullptr;
        std::function<void(View&)> completion = {};
    };

    struct CloseAllCommand {};

    explicit ViewManager(ServiceContext& service_context);
    ViewManager(ViewManager const&) = delete;
    ViewManager& operator=(ViewManager const&) = delete;
    ~ViewManager();

    /// Creates an instance of the view \p entry
    View* open_view(ViewRegistryEntry const& entry);

    /// Focuses the existing view with static name \p name or, if nonesuch
    /// exists, finds the view registry entry with name \p name and creates a
    /// new view
    View* open_view(std::string_view name,
                    std::function<void(View&)> completion = {});

    /// Finds and returns the first open view with static name \p name or null
    /// if none exists
    View* find_by_static_name(std::string_view name);

    /// Closes all open views
    void close_all();

    ///
    void close_focused_view();

    /// Displays all views
    void display();

    ///
    bool on_input(InputEvent& event);

    /// Serializes the currently open views
    YAML::Node serialize() const;

    /// Recreates the views stored in \p node
    void deserialize(YAML::Node const& node);

    /// \returns the currently focused view if any view is focused or null
    View* focused_view() {
        return const_cast<View*>(std::as_const(*this).focused_view());
    }

    /// \overload
    View const* focused_view() const;

private:
    struct Impl;
    EventReceiver event_receiver;
    ServiceContext& service_context;
    std::vector<std::unique_ptr<View>> views;
    bool display_debugger = false;
};

} // namespace mosaic
