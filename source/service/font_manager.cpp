#include "service/font_manager.h"

#include <array>
#include <fstream>
#include <utility>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "application/resource_util.h"
#include "support/logger.h"

using namespace mosaic;

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
    if (font.style == FontStyle::monospaced) sstr << "SFMono";
    else sstr << "SFPro";
    sstr << "-" << to_string(font.weight) << "-";
    if (font.style == FontStyle::monospaced)
        sstr << to_string(FontStyle::roman);
    else sstr << to_string(font.style);
    sstr << ".ttf";
    return std::move(sstr).str();
}

static utl::hashmap<std::string, uint16_t> load_all_icon_codes() {
    auto resource_path = resource_directory() / "icons/icons_config.json";
    std::fstream file(resource_path, std::ios::in);
    if (!file) {
        Logger::error("Failed to load icon resource file: ", resource_path, " ",
                      strerror(errno));
        return {};
    }
    nlohmann::json doc;
    file >> doc;
    utl::hashmap<std::string, uint16_t> codes;
    for (auto const& glyph : doc["glyphs"]) {
        auto css_name = glyph["css"].get<std::string>();
        auto code = glyph["code"].get<unsigned>();
        if (code > USHRT_MAX) {
            Logger::warn("Skipping icon (unicode value exceeds range): ",
                         css_name);
            continue;
        }
        codes.insert({std::move(css_name), utl::narrow_cast<uint16_t>(code)});
    }
    return codes;
}

FontManager::FontManager(float scale_factor)
    : all_icon_codes(load_all_icon_codes())
    , scale_factor(scale_factor) {}

static ImFont* load_font(FontDesc const& desc, ImFontAtlas& atlas,
                         float scale_factor) {
    if (desc.style == FontStyle::monospaced &&
        (int)desc.weight < (int)FontWeight::light) {
        return nullptr;
    }
    if (desc.style == FontStyle::monospaced &&
        (int)desc.weight > (int)FontWeight::heavy) {
        return nullptr;
    }
    auto path = resource_directory() / "font" / to_font_file_name(desc);
    auto* imgui_font_ptr =
        atlas.AddFontFromFileTTF(path.c_str(), (int)desc.size * scale_factor);
    assert(imgui_font_ptr);
    return imgui_font_ptr;
}

ImFont* FontManager::get_font(FontDesc const& desc) {
    auto [itr, inserted] = font_map.insert({desc, nullptr});
    if (inserted) itr->second = load_font(desc, *atlas, scale_factor);
    return itr->second;
}

static ImFont* load_icon_font(IconFontDesc const& desc, ImFontAtlas& atlas,
                              float scale_factor) {
    auto path = resource_directory() / "icons/icons.ttf";
    auto* font = atlas.AddFontFromFileTTF(
        path.c_str(), static_cast<float>(desc.size) * scale_factor);
    assert(font);
    return font;
}

static IconData::StringType unicode_to_utf8(uint16_t code) {
    IconData::StringType result{};
    int count =
        ImTextStrToUtf8(result.data(), (int)result.size(), &code, &code + 1);
    assert(count < (int)result.size());
    return result;
}

IconData FontManager::get_icon(IconFontDesc const& desc,
                               std::string_view name) {
    auto code_itr = all_icon_codes.find(name);
    if (code_itr == all_icon_codes.end()) return {};
    auto code = code_itr->second;
    auto [font_itr, inserted] = icon_font_map.insert({desc, nullptr});
    if (inserted) font_itr->second = load_icon_font(desc, *atlas, scale_factor);
    return {
        font_itr->second,
        unicode_to_utf8(code),
    };
}
