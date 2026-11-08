#pragma once

#include <string>

#include <mosaic/service/application_command.h>
#include <mosaic/tool/tool_class.h>

namespace mosaic {

struct DemoToolOpenData { std::string message; };

ApplicationCommand demo_tool_open_command(ToolOpenPolicy policy,
                                          DemoToolOpenData open_data);

} // namespace mosaic