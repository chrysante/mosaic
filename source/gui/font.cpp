#include "gui/font.h"

#include <algorithm>
#include <array>
#include <fstream>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "application/resource_util.h"
#include "support/logger.h"

using namespace mosaic;

struct FontManager::Impl {
    using FontKey = FontManager::FontKey;
    using ImGuiData = FontManager::ImGuiData;
};

using FontKey = FontManager::Impl::FontKey;
using ImGuiData = FontManager::Impl::ImGuiData;

static std::string to_string(FontWeight w) {
    return std::array{
        "Ultralight", "Thin", "light", "regular", "medium",
        "Semibold",   "Bold", "heavy", "Black"}[utl::to_underlying(w)];
}

static std::string to_string(FontStyle s) {
    return std::array{"roman", "Italic"}[utl::to_underlying(s)];
}

static std::string to_font_file_name(FontDesc const& font) {
    std::stringstream sstr;
    if (font.style == FontStyle::monospaced) {
        sstr << "SFMono";
    } else {
        sstr << "SFPro";
    }
    sstr << "-" << to_string(font.weight) << "-";
    if (font.style == FontStyle::monospaced) {
        sstr << to_string(FontStyle::roman);
    } else {
        sstr << to_string(font.style);
    }
    sstr << ".ttf";
    return std::move(sstr).str();
}

static ImFont* load_font(FontDesc const& font, ImFontAtlas& atlas,
                         float scale_factor) {
    if (font.style == FontStyle::monospaced &&
        (int)font.weight < (int)FontWeight::light) {
        return nullptr;
    }
    if (font.style == FontStyle::monospaced &&
        (int)font.weight > (int)FontWeight::heavy) {
        return nullptr;
    }
    auto path = resource_directory() / "Font" / to_font_file_name(font);
    auto* imgui_font_ptr =
        atlas.AddFontFromFileTTF(path.c_str(), (int)font.size * scale_factor);
    assert(imgui_font_ptr);
    return imgui_font_ptr;
}

static void load_fonts(ImGuiData& data, float scale_factor) {
    for (auto& [key, ptr] : data.map) {
        if (std::holds_alternative<FontDesc>(key)) {
            ptr = load_font(std::get<FontDesc>(key), *data.atlas, scale_factor);
        }
    }
}

static void load_icons(
    ImGuiData& data, float scale_factor,
    utl::hashmap<IconSize, std::vector<uint16_t>> const& sorted_glyph_map) {
    std::filesystem::path icon_path = resource_directory() / "icons/icons.ttf";
    for (auto& [size, glyphs] : sorted_glyph_map) {
        ImFont* imgui_font_ptr = data.atlas->AddFontFromFileTTF(
            icon_path.c_str(), (int)size * scale_factor, nullptr,
            glyphs.data());
        data.map[FontKey{IconFontDesc{size}}] = imgui_font_ptr;
    }
}

static utl::hashmap<std::string, uint16_t> load_all_icon_codes() {
    auto resourcePath = resource_directory() / "icons/icons_config.json";
    std::fstream file(resourcePath, std::ios::in);
    if (!file) {
        Logger::error("Failed to load icon resource file: ", resourcePath, " ",
                      strerror(errno));
        return {};
    }
    assert(false);
#if 0
    rapidjson::IStreamWrapper stream(file);
    rapidjson::Document doc;
    doc.ParseStream(stream);
    utl::hashmap<std::string, uint16_t> codes;
    for (auto const& glyph : doc["glyphs"]) {
        auto cssName = glyph["css"].GetString();
        auto code = glyph["code"].GetUint();
        if (code > USHRT_MAX) {
            Logger::Warn("Skipping icon (unicode values exeeds range): ",
                         cssName);
            continue;
        }
        codes.insert({std::move(cssName), utl::narrow_cast<WChar>(code)});
    }
    return codes;
#endif
}

static void load_all(
    ImGuiData& data, float scale_factor,
    utl::hashmap<IconSize, std::vector<uint16_t>> const& sorted_glyph_map) {
    if (!data.atlas)
        data.atlas = IM_NEW(ImFontAtlas);
    data.atlas->Clear();
    load_fonts(data, scale_factor);
    load_icons(data, scale_factor, sorted_glyph_map);
    data.atlas->Build();
}

FontDesc FontDesc::ui_default() {
    FontDesc font{};
    font.size = FontSize::medium;
    font.weight = FontWeight::regular;
    font.style = FontStyle::roman;
    return font;
}

FontDesc FontDesc::mono_default() {
    FontDesc font{};
    font.size = FontSize{14};
    font.weight = FontWeight::regular;
    font.style = FontStyle::monospaced;
    return font;
}

FontManager::FontManager(float scale_factor)
    : all_icon_codes(load_all_icon_codes())
    , imgui_data{.atlas = IM_NEW(ImFontAtlas)}
    , scale_factor(scale_factor) {
    imgui_data.map.insert({FontDesc::ui_default(), nullptr});
}

FontManager::~FontManager() {
    if (imgui_data.atlas) {
        IM_FREE(imgui_data.atlas);
    }
}

static std::unique_ptr<FontManager> g_font_manager = nullptr;

void FontManager::set_global_instance(
    std::unique_ptr<FontManager> fontManager) {
    g_font_manager = std::move(fontManager);
}

FontManager* FontManager::get_global_instance() {
    return g_font_manager.get();
}

ImFont* FontManager::get(FontDesc const& desc) {
    return g_font_manager->get_impl(desc);
}

IconData FontManager::get(IconFontDesc const& desc, std::string name) {
    return {g_font_manager->get_impl(desc),
            g_font_manager->get_unicode_str_impl(desc.size, std::move(name))};
}

static utl::hashmap<IconSize, std::vector<uint16_t>> make_sorted_glyph_map(
    utl::hashmap<IconSize, utl::hashset<uint16_t>> const& icon_glyphs) {
    utl::hashmap<IconSize, std::vector<uint16_t>> result;
    for (auto& [key, set] : icon_glyphs) {
        auto& glyphs = result[key];
        glyphs = set.values();
        std::sort(glyphs.begin(), glyphs.end());
        glyphs.push_back(0); // Null terminator
    }
    return result;
}

ImFont* FontManager::get_impl(FontKey const& key) {
    auto [itr, inserted] = imgui_data.map.insert({key, {nullptr}});
    if (inserted) {
        reload_async();
    }
    return itr->second;
}

static std::string unicode_to_utf8(uint16_t code) {
    std::array<char, 16> result{};
    int count = ImTextStrToUtf8(result.data(), 16, &code, &code + 1);
    assert(count < 16);
    if (count == 0)
        return {};
    return std::string(result.data());
}

std::string FontManager::get_unicode_str_impl(IconSize size,
                                              std::string_view name) {
    auto itr = all_icon_codes.find(name);
    if (itr == all_icon_codes.end()) {
        return {};
    }
    auto code = itr->second;
    if (glyph_map[size].insert(code).second) {
        reload_async();
    }
    return unicode_to_utf8(code);
}

void FontManager::reload_async() {
    if (have_signaled_reload)
        return;
    have_signaled_reload = true;
    /// We start the async operation next frame because when a new view opens
    /// that uses multiple unavailable fonts, we want to gather all fonts we
    /// need to load and then start the reload operation

    // dispatch(DispatchToken::NextFrame, [this] { start_reload_thread(); });
}

void FontManager::start_reload_thread() {
#if 0
    if (is_reloading) 
    return;
    have_signaled_reload = false;
    is_reloading = true;
    std::thread([this, scale_factor = scale_factor,
                sorted_glyph_map = make_sorted_glyph_map(glyph_map),
                data = imgui_data]() mutable {
        data.atlas = IM_NEW(ImFontAtlas);
        load_all(data, scale_factor, sorted_glyph_map);
        postEmptySystemEvent();
        dispatch(DispatchToken::NextFrame, [this, data = std::move(data)] {
            complete_reloading_sync(std::move(data));
        });
    }).detach();
#endif
}

static void amend_map(utl::hashmap<FontKey, ImFont*>& dest,
                      utl::hashmap<FontKey, ImFont*> const& source) {
    dest.insert(source.begin(), source.end());
}

void FontManager::complete_reloading_sync(ImGuiData data) {
    is_reloading = false;
    IM_FREE(imgui_data.atlas);
    imgui_data.atlas = data.atlas;
    /// We amend the new map with all entries that were added during the built
    amend_map(data.map, imgui_data.map);
    imgui_data.map = std::move(data.map);
    // dispatch(DispatchToken::Now, ReloadedFontAtlasCommand{this});
    if (have_signaled_reload)
        start_reload_thread();
}
