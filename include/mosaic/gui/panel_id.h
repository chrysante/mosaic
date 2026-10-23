#pragma once

#include <cstdint>
#include <string>

#include <vml/vml.hpp>

namespace mosaic {

struct PanelClass {
    std::string name;

    vml::float2 default_position = {250, 250};
    vml::float2 default_size = {600, 400};
    vml::float2 min_size = {300, 200};
};

struct PanelId {
    PanelClass const* panel_class = nullptr;
    std::uint64_t instance = 0;

    friend bool operator==(PanelId const&, PanelId const&) = default;
};

} // namespace mosaic