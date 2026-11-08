#pragma once

#include <any>
#include <concepts>
#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/tool/tool_class.h>

namespace mosaic {

class ServiceContext;
class Tool;

class ToolManager {
public:
    explicit ToolManager(ServiceContext& service_context);
    ~ToolManager();

    template <typename ToolId, typename ToolData>
    requires std::same_as<typename ToolData::ToolId, ToolId> Tool*
    open_tool(ToolOpenPolicy policy, std::any data) {
        return open_tool(&ToolId::tool_class, policy, std::move(data));
    }

    Tool* open_tool(ToolClass const* tool_class, ToolOpenPolicy policy,
                    std::any data = {});

    void update();

    nlohmann::json save_config() const;

    void load_config(nlohmann::json const&);

#ifndef NDEBUG
    void draw_debugger();
#endif // NDEBUG

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
};

} // namespace mosaic