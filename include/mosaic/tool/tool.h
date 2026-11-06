#pragma once

#include <cassert>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/gui/command_palette.h>
#include <mosaic/tool/tool_class.h>

namespace mosaic {

class ServiceContext;
class ToolManager;

class Tool {
public:
    virtual ~Tool() = default;

    ToolId tool_id() const { return m_tool_id; }

    ServiceContext& service_context() const {
        assert(m_service_context);
        return *m_service_context;
    }

    CommandPalette const& command_palette() const { return m_command_palette; }

protected:
    Tool() = default;

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
    CommandPalette m_command_palette;
};

} // namespace mosaic