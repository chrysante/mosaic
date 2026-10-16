#pragma once

#include <concepts>

#include <imgui.h>
#include <utl/hash.hpp>
#include <utl/overload.hpp>
#include <utl/scope_guard.hpp>

namespace mosaic {

enum class FontWeight {
    ultralight,
    thin,
    light,
    regular,
    medium,
    semibold,
    bold,
    heavy,
    black
};

enum class FontStyle { roman, italic, monospaced };

template <typename T>
concept FontParameterType =
    std::is_same_v<T, FontWeight> || std::is_same_v<T, FontStyle>;

/// Font attribute descriptor. Used to identify fonts when interacting with the
/// font manager
struct FontDesc {
    FontWeight weight = FontWeight::regular;
    FontStyle style = FontStyle::roman;

    /// \returns the font descriptor used for most UI elements
    static FontDesc ui_default();

    /// \overload
    static FontDesc ui_default(FontParameterType auto... args) {
        return ui_default().apply(args...);
    }

    /// \returns the font descriptor for the default monospaced font
    static FontDesc mono_default();

    /// Modifies the wheight value of this font descriptor
    FontDesc& set_weight(FontWeight weight) {
        this->weight = weight;
        return *this;
    }

    /// Modifies the style value of this font descriptor
    FontDesc& set_style(FontStyle style) {
        this->style = style;
        return *this;
    }

    /// Modifies this font descriptor by setting the given arguments
    FontDesc& apply(FontParameterType auto... args) {
        utl::overload apply = {
            [&](FontWeight weight) { set_weight(weight); },
            [&](FontStyle style) { set_style(style); },
        };
        (apply(args), ...);
        return *this;
    }

    bool operator==(FontDesc const& rhs) const = default;
};

/// Result structure of `FontManager::get_icon(...)`
struct IconData {
    using StringType = std::array<char, 16>;

    /// The font that encodes the icon
    ImFont* font;

    /// The string that represents the icon
    StringType utf8_rep{};
};

namespace ui {

/// Tries to return the font described by \p desc
/// If the font is not available, it will be loaded in the background. In
/// this case another font is returned and the requested font will be
/// returned by the next call to `get` as soon as the font is available
ImFont* font(FontDesc const& desc);

/// Convenience overload
ImFont* font(FontParameterType auto... params) {
    return ui::font(FontDesc::ui_default(params...));
}

///
IconData icon_data(std::string_view name);

decltype(auto) with_font(ImFont* font, float size,
                         std::invocable auto&& block) {
    ImGui::PushFont(font, size);
    utl::scope_guard pop = [] { ImGui::PopFont(); };
    return block();
}

decltype(auto) with_font(ImFont* font, std::invocable auto&& block) {
    return with_font(font, 0.0f, std::forward<decltype(block)>(block));
}

decltype(auto) with_font(FontDesc const& desc, std::invocable auto&& block) {
    return with_font(ui::font(desc), block);
}

decltype(auto) with_font(FontDesc const& desc, float size,
                         std::invocable auto&& block) {
    return with_font(ui::font(desc), size, block);
}

} // namespace ui

} // namespace mosaic

template <> struct std::hash<mosaic::FontDesc> {
    std::size_t operator()(mosaic::FontDesc const& desc) const {
        return utl::hash_combine(desc.weight, desc.style);
    }
};
