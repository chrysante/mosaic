#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/service/event_receiver.h>

namespace mosaic {

class ServiceContext;
class View;
class ViewRegistryEntry;
class ImGuiViewRenderer;
struct InputEvent;
struct ViewId;

class ViewManager {
public:
    struct OpenCommand {
        std::string name;
        std::function<void(View&)> completion;
    };

    struct CloseAllCommand {};
    struct ToggleDebugView {};

    explicit ViewManager(ServiceContext& service_context);
    ~ViewManager();

    ViewManager(ViewManager const&) = delete;
    ViewManager& operator=(ViewManager const&) = delete;

    View* open_view(ViewRegistryEntry const& entry,
                    std::function<void(View&)> completion = {});

    View* open_view(std::string_view name,
                    std::function<void(View&)> completion = {});

    View* find_by_id(ViewId const& id);
    View const* find_by_id(ViewId const& id) const;

    View* find_by_type(std::string_view type);
    View const* find_by_type(std::string_view type) const;

    void focus(View& view);

    void close(View& view);
    void close_focused_view();
    void close_all();

    void display();
    bool on_input(InputEvent& event);
    nlohmann::json save_settings() const;
    void load_settings(nlohmann::json const&);

    View* focused_view();
    View const* focused_view() const;

private:
    friend class ImGuiViewRenderer;

    uint64_t allocate_instance_id(std::string_view type);

    void process_deferred_operations();

    void display_debugger();

    ServiceContext& service_context;
    EventReceiver event_receiver;

    std::vector<std::unique_ptr<View>> views;

    View* focused = nullptr;

    std::unordered_map<std::string, uint64_t> next_instance_ids;

    std::unique_ptr<ImGuiViewRenderer> renderer;
};

} // namespace mosaic