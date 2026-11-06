#include "support/geometry.h"

#include <nlohmann/json.hpp>

#include "support/json_support.h"

using namespace mosaic;

void mosaic::to_json(nlohmann::json& j, Rect const& r) {
    j["origin"] = r.origin;
    j["size"] = r.size;
}

void mosaic::from_json(nlohmann::json const& j, Rect& r) {
    j.at("origin").get_to(r.origin);
    j.at("size").get_to(r.size);
}
