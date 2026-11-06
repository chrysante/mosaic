#include "tool/tool.h"

#include <nlohmann/json.hpp>

#include "gui/panel.h" // IWYU pragma: keep

using namespace mosaic;

Tool::Tool() = default;

Tool::~Tool() = default;

nlohmann::json Tool::save_config() const { return nlohmann::json::object(); }