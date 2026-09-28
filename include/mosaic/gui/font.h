#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include <imgui.h>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/overload.hpp>

namespace mosaic {

class Application;

enum class FontSize { medium = 17, Large = 34 };

constexpr FontSize default_font_size = FontSize::medium;

template <typename T>
requires std::is_arithmetic_v<T> FontSize operator+(FontSize s, T t) {
    return FontSize{(T)s + t};
}

template <typename T>
requires std::is_arithmetic_v<T> FontSize operator+(T t, FontSize s) {
    return FontSize{t + (T)s};
}

template <typename T>
requires std::is_arithmetic_v<T> FontSize operator-(FontSize s, T t) {
    return FontSize{(T)s - t};
}

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
    std::is_same_v<T, FontSize> || std::is_same_v<T, FontWeight> ||
    std::is_same_v<T, FontStyle>;

/// Font attribute descriptor. Used to identify fonts when interacting with the
/// font manager
struct FontDesc {
    FontSize size = FontSize::medium;
    FontWeight weight = FontWeight::regular;
    FontStyle style = FontStyle::roman;

    /// \Returns the font descriptor used for most UI elements
    static FontDesc ui_default();

    /// \overload
    static FontDesc ui_default(FontParameterType auto... args) {
        return ui_default().apply(args...);
    }

    ///
    static FontDesc mono_default();

    ///
    FontDesc& set_size(FontSize size) {
        this->size = size;
        return *this;
    }

    FontDesc& set_weight(FontWeight weight) {
        this->weight = weight;
        return *this;
    }

    FontDesc& set_style(FontStyle style) {
        this->style = style;
        return *this;
    }

    FontDesc& apply(FontParameterType auto... args) {
        utl::overload apply = {
            [&](FontSize size) { set_size(size); },
            [&](FontWeight weight) { set_weight(weight); },
            [&](FontStyle style) { set_style(style); },
        };
        (apply(args), ...);
        return *this;
    }

    bool operator==(FontDesc const& rhs) const = default;
};

enum class IconSize { _16 = 16, _24 = 24, _32 = 32, _48 = 48, _64 = 64 };

struct IconFontDesc {
    IconSize size;

    bool operator==(IconFontDesc const& rhs) const = default;
};

} // namespace mosaic

template <> struct std::hash<mosaic::FontDesc> {
    std::size_t operator()(mosaic::FontDesc const& desc) const {
        return utl::hash_combine(desc.size, desc.weight, desc.style);
    }
};

template <> struct std::hash<mosaic::IconFontDesc> {
    std::size_t operator()(mosaic::IconFontDesc const& desc) const {
        return std::hash<mosaic::IconSize>{}(desc.size);
    }
};

namespace mosaic {

/// Result structure of `FontManager::get(IconFontDesc desc, std::string name)`
struct IconData {
    /// The font that encodes the icon
    ImFont* font;

    /// The string that represents the icon
    std::string utf8_rep;
};

///
class FontManager {
public:
    explicit FontManager(float scale_factor);

    ~FontManager();

    ///
    static void set_global_instance(std::unique_ptr<FontManager> fontManager);

    ///
    static FontManager* get_global_instance();

    /// Tries to return the font described by \p desc
    /// If the font is not available, it will be loaded in the background. In
    /// this case another font is returned and the requested font will be
    /// returned by the next call to `get` as soon as the font is available
    static ImFont* get(FontDesc const& font);

    /// Convenience overload
    static ImFont* get(FontParameterType auto... params) {
        return get(FontDesc::ui_default(params...));
    }

    ///
    static IconData get(IconFontDesc const& font, std::string icon_name);

    ///  \Returns the ImGui font atlas
    ImFontAtlas* get_atlas() {
        return imgui_data.atlas;
    }

    struct Impl;

private:
    using FontKey = std::variant<FontDesc, IconFontDesc>;

    struct ImGuiData {
        ImFontAtlas* atlas = nullptr;
        utl::hashmap<FontKey, ImFont*> map;
    };

    ImFont* get_impl(FontKey const& key);

    std::string get_unicode_str_impl(IconSize size, std::string_view name);

    void reload_async();
    void start_reload_thread();
    void complete_reloading_sync(ImGuiData data);

    /// Maps CSS icon names to unicode representation. This lists all icons
    /// available in the resource files
    utl::hashmap<std::string, uint16_t> all_icon_codes;
    utl::hashmap<IconSize, utl::hashset<uint16_t>> glyph_map;
    ImGuiData imgui_data;
    float scale_factor = 1;
    bool have_signaled_reload = false;
    bool is_reloading = false;
};

struct ReloadedFontAtlasEvent {
    // FontManager* fontManager;
};

} // namespace mosaic
