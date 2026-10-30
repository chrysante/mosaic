#pragma once

#include <optional>

#include <utl/hash.hpp>
#include <utl/overload.hpp>
#include <utl/scope_guard.hpp>

// ImGui font type. Forward declare this here so we don't need to include
// imgui.h
struct ImFont;

namespace mosaic {

enum class FontWeight {
    ultralight = 0,
    thin = 1,
    light = 2,
    regular = 3,
    medium = 4,
    semibold = 5,
    bold = 6,
    heavy = 7,
    black = 8
};

enum class FontStyle { roman, italic, monospaced };

/// Font attribute descriptor. Used to identify fonts when interacting with the
/// font manager
struct FontDesc {
    FontWeight weight;
    FontStyle style;

    // For using FontDesc as keys in a hashtable
    bool operator==(FontDesc const&) const = default;
};

struct FontContext {
    FontDesc desc;
    /// Absolute size
    float size;
    ImFont* font;
};

struct FontModifier {
    std::optional<FontWeight> weight_override = std::nullopt;
    int weight_offset = 0;

    std::optional<FontStyle> style_override;

    /// This is in multiples of the base font size
    float size_override = 0.0f;
    float size_add = 0.0f;
    float size_scale = 1.0f;
};

namespace ui {

/// Returns the top of the font-stack. This is tied to the current ImGui context
/// and must not be called without an active context
FontContext get_current_font();

void push_font(FontModifier mod);

/// \p size is in multiples of the base font size.
void push_font(FontDesc desc, float size);

void pop_font();

decltype(auto) with_font(FontDesc desc, float size,
                         std::invocable auto&& block) {
    push_font(desc, size);
    utl::scope_guard pop = [] { pop_font(); };
    return std::invoke(block);
}

decltype(auto) with_font(FontModifier mod, std::invocable auto&& block) {
    push_font(mod);
    utl::scope_guard pop = [] { pop_font(); };
    return std::invoke(block);
}

decltype(auto) with_font(FontDesc desc, std::invocable auto&& block) {
    return with_font(FontModifier{.weight_override = desc.weight,
                                  .style_override = desc.style},
                     std::forward<decltype(block)>(block));
}

} // namespace ui

} // namespace mosaic

template <> struct std::hash<mosaic::FontDesc> {
    std::size_t operator()(mosaic::FontDesc const& desc) const {
        return utl::hash_combine(desc.weight, desc.style);
    }
};

// =============================================================================
// Icons
// =============================================================================

// TODO: Rework the whole icon system

namespace mosaic {

/// Result structure of `FontManager::get_icon(...)`
struct IconData {
    using StringType = std::array<char, 16>;

    /// The font that encodes the icon
    ImFont* font;

    /// The string that represents the icon
    StringType utf8_rep{};
};

namespace ui {

///
IconData icon_data(std::string_view name);

} // namespace ui

} // namespace mosaic
