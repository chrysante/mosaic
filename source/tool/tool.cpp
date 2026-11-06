#include "tool/tool.h"

#include <nlohmann/json.hpp>

using namespace mosaic;

nlohmann::json Tool::save_config() const { return nlohmann::json::object(); }