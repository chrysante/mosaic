#include "service/font_manager.h"

#include <array>
#include <utility>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "application/directories.h"
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

FontManager::FontManager() = default;

static ImFont* load_font(FontDesc const& desc, ImFontAtlas& atlas) {
    if (desc.style == FontStyle::monospaced &&
        (int)desc.weight < (int)FontWeight::light) {
        Logger::error("Font not available");
        return nullptr;
    }
    if (desc.style == FontStyle::monospaced &&
        (int)desc.weight > (int)FontWeight::heavy) {
        Logger::error("Font not available");
        return nullptr;
    }
    auto path = resource_directory() / "font" / to_font_file_name(desc);
    auto* imgui_font_ptr = atlas.AddFontFromFileTTF(path.c_str());
    assert(imgui_font_ptr);
    return imgui_font_ptr;
}

ImFont* FontManager::get_font(FontDesc const& desc) {
    auto [itr, inserted] = m_font_map.insert({desc, nullptr});
    if (inserted) itr->second = load_font(desc, *m_atlas);
    return itr->second;
}
