#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>
#include <utl/hashtable.hpp>

#include <mosaic/gui/view_id.h>
#include <mosaic/service/event_receiver.h>

namespace mosaic {

class ServiceContext;
struct InputEvent;
class View;
class ViewRegistryEntry;
class ImGuiViewRenderer;

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

    View* find_by_type(ViewClass const& view_class) {
        return const_cast<View*>(std::as_const(*this).find_by_type(view_class));
    }
    View const* find_by_type(ViewClass const& view_class) const;

    View* find_by_id(ViewId view_id) {
        return const_cast<View*>(std::as_const(*this).find_by_id(view_id));
    }
    View const* find_by_id(ViewId view_id) const;

    void close_all();

    void display();

    bool on_input(InputEvent& event);

    nlohmann::json save_settings() const;

    void load_settings(nlohmann::json const&);

    View* focused_view() { return m_focused_view; }

    View const* focused_view() const { return m_focused_view; }

#ifndef NDEBUG
    void draw_debugger();
#endif

private:
    friend class ImGuiViewRenderer;

    uint64_t new_instance_id(ViewClass const* type) const;

    View* insert_view(ViewId view_id, std::unique_ptr<View> view);

    void process_deferred_operations();

    using ViewMap = utl::hashmap<std::uint64_t, std::unique_ptr<View>>;

    ServiceContext& service_context;
    EventReceiver event_receiver;
    std::unique_ptr<ImGuiViewRenderer> renderer;
    utl::hashmap<ViewClass const*, ViewMap> m_views;
    View* m_focused_view = nullptr;
};

} // namespace mosaic