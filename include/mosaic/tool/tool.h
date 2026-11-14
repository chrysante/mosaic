#pragma once

#include <cassert>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/gui/command_palette.h>
#include <mosaic/service/application_command.h>
#include <mosaic/tool/tool_class.h>
#include <mosaic/tool/tool_registry.h>

namespace mosaic {

class ServiceContext;
class ToolManager;

class Tool {
public:
    virtual ~Tool();

    ToolId id() const { return m_tool_id; }

    ServiceContext& service_context() const {
        assert(m_service_context);
        return *m_service_context;
    }

    CommandPalette const& command_palette() const { return m_command_palette; }

    void request_kill() { m_kill_requested = true; }

protected:
    Tool();

    CommandPalette& command_palette_write() { return m_command_palette; }

private:
    friend class ToolManager;
    friend class ToolRegistry;

    /// Called every frame
    virtual void update() = 0;

    /// Called once after construction and framework initialization.
    virtual void on_create() {}

    /// Called once before destruction.
    virtual void on_destroy() {}

    /// Config persistance
    virtual nlohmann::json save_config() const;
    virtual void load_config(nlohmann::json const&) {}

    ToolId m_tool_id;
    ServiceContext* m_service_context = nullptr;
    bool m_kill_requested = false;
    CommandPalette m_command_palette;
};

int to_panel_id(ToolId const& id);

namespace internal {
Tool* create_tool(Application& app, ToolClass const* tool_class,
                  ToolOpenPolicy policy);
}

template <typename ToolType, std::invocable<ToolType&> Callback>
ApplicationCommand make_tool_open_command(ToolOpenPolicy policy,
                                          Callback callback) {
    return ApplicationCommand::create(
        [policy, callback = std::move(callback)](Application& app) {
            Tool* t = mosaic::internal::create_tool(app, &ToolType::tool_class,
                                                    policy);
            auto* tool = dynamic_cast<ToolType*>(t);
            assert(tool);
            callback(*tool);
        });
}

} // namespace mosaic