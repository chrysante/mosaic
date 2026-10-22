#pragma once

#include <cfloat>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <variant>

#include <utl/function_view.hpp>
#include <utl/vector.hpp>

namespace mosaic::ui {

// =============================================================================
// Columns
// =============================================================================

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

// =============================================================================
// Properties
// =============================================================================

void begin_properties(char const* str_id);
void section(char const* title = nullptr);
void end_properties();

namespace internal {
void item_impl(const char* label,
               std::span<utl::function_view<void()> const> line_callbacks);
}

template <std::invocable... F>
requires(sizeof...(F) > 0) void item(const char* label, F&&... lines) {
    utl::small_vector<utl::function_view<void()>, 4> line_callbacks = {
        lines...};
    internal::item_impl(label, line_callbacks);
}

#ifndef NDEBUG
void draw_properties_debugger();
#endif

} // namespace mosaic::ui