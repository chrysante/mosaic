#pragma once

#include <cfloat>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <variant>

namespace mosaic::ui {

struct ColumnLayoutOptions {
    // Negative value -> auto-resolve
    // 0              -> no headers
    // positive value -> manual override
    float header_height = -1.0f;
};

struct ColumnLayout {
    using Header = std::variant<std::string, std::function<void()>>;

    Header header;
    std::function<void()> content;
    float min_width = 10.0f;
    float max_width = FLT_MAX;
};

void columns(char const* str_id, std::span<ColumnLayout const> columns,
             ColumnLayoutOptions options = {});

void columns(char const* str_id, std::initializer_list<ColumnLayout> columns,
             ColumnLayoutOptions options = {});

} // namespace mosaic::ui