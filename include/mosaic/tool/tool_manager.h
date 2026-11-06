#pragma once

#include "service/service_context.h"
#include <any>
#include <concepts>
#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/tool/tool_class.h>

namespace mosaic {

namespace internal { struct ToolManagerImpl; }

enum class ToolOpenPolicy { open, open_or_find };

class ToolManager {
public:
    explicit ToolManager(ServiceContext& service_context);
    ~ToolManager();

    template <typename ToolId, typename ToolData>
    requires std::same_as<typename ToolData::ToolId, ToolId> void
    open_tool(ToolOpenPolicy policy, std::any data) {
        open_tool(&ToolId::tool_class, policy, std::move(data));
    }

    void open_tool(ToolClass const* tool_class, ToolOpenPolicy policy,
                   std::any data);

    void update();

    nlohmann::json save_config() const;

    void load_config(nlohmann::json const&);

private:
    std::unique_ptr<internal::ToolManagerImpl> m_impl;
};

} // namespace mosaic