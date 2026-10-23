#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>
#include <utl/hashtable.hpp>

#include <mosaic/gui/panel_id.h>
#include <mosaic/service/event_receiver.h>

namespace mosaic {

class ServiceContext;
struct InputEvent;
class Panel;
class PanelRegistryEntry;
class PanelRenderer;

class PanelManager {
public:
    struct OpenCommand {
        std::string name;
        std::function<void(Panel&)> completion;
    };

    struct CloseAllCommand {};
    struct ToggleDebugView {};

    explicit PanelManager(ServiceContext& service_context);
    ~PanelManager();

    PanelManager(PanelManager const&) = delete;
    PanelManager& operator=(PanelManager const&) = delete;

    Panel* open_panel(PanelRegistryEntry const& entry,
                      std::function<void(Panel&)> completion = {});

    Panel* open_panel(std::string_view name,
                      std::function<void(Panel&)> completion = {});

    Panel* find_by_type(PanelClass const& panel_class) {
        return const_cast<Panel*>(
            std::as_const(*this).find_by_type(panel_class));
    }
    Panel const* find_by_type(PanelClass const& panel_class) const;

    Panel* find_by_id(PanelId panel_id) {
        return const_cast<Panel*>(std::as_const(*this).find_by_id(panel_id));
    }
    Panel const* find_by_id(PanelId panel_id) const;

    void close_all();

    void display();

    bool on_input(InputEvent& event);

    nlohmann::json save_settings() const;

    void load_settings(nlohmann::json const&);

    Panel* focused_panel() { return m_focused_panel; }

    Panel const* focused_panel() const { return m_focused_panel; }

#ifndef NDEBUG
    void draw_debugger();
#endif

private:
    friend class PanelRenderer;

    uint64_t new_instance_id(PanelClass const* panel_class) const;

    Panel* insert_panel(PanelId panel_id, std::unique_ptr<Panel> panel);

    void process_deferred_operations();

    using PanelMap = utl::hashmap<std::uint64_t, std::unique_ptr<Panel>>;

    ServiceContext& service_context;
    EventReceiver event_receiver;
    std::unique_ptr<PanelRenderer> renderer;
    utl::hashmap<PanelClass const*, PanelMap> m_panels;
    Panel* m_focused_panel = nullptr;
};

} // namespace mosaic