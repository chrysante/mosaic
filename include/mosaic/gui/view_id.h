#pragma once

#include <cstdint>
#include <string>

#include <vml/vml.hpp>

namespace mosaic {

struct ViewClass {
    std::string name;

    vml::float2 default_position = {250, 250};
    vml::float2 default_size = {600, 400};
    vml::float2 min_size = {300, 200};
};

struct ViewId {
    ViewClass const* view_class = nullptr;
    std::uint64_t instance = 0;

    friend bool operator==(ViewId const&, ViewId const&) = default;
};

} // namespace mosaic