#pragma once

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

    Tool* open_tool(ToolClass const* tool_class, ToolOpenPolicy policy);

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