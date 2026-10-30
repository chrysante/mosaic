#include "service/icon_manager.h"

#include <fstream>

#include <imgui.h>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include "application/directories.h"
#include "gui/icon.h"
#include "gui/ui_context.h"
#include "support/logger.h"

using namespace mosaic;

std::vector<IconId>
IconManager::add_font_icon_set(ImFont* font,
                               std::span<unsigned const> code_points) {
    std::vector<IconId> result;
    result.reserve(code_points.size());
    for (unsigned code_point : code_points) {
        IconId const icon_id = {.value = m_icon_id_counter++};
        m_icons.emplace(icon_id, FontIcon{font, code_point});
        result.push_back(icon_id);
    }
    return result;
}

Icon IconManager::get_icon(IconId icon_id) {
    auto itr = m_icons.find(icon_id);
    return itr != m_icons.end() ? itr->second : m_fallback_icon;
}

void mosaic::load_default_icons(IconManager& icon_manager) {
    auto code_points_filepath =
        resource_directory() / "icons/lucide-codepoints.json";
    auto font_filepath = resource_directory() / "icons/lucide.ttf";
    // Load codepoint mapping
    std::ifstream file(code_points_filepath);
    if (!file) {
        Logger::error("Failed to open Lucide codepoints file: {}",
                      code_points_filepath.string());
        return;
    }
    nlohmann::json mapping;
    try {
        file >> mapping;
    } catch (nlohmann::json::exception const& e) {
        Logger::error("Failed to parse Lucide codepoints file: {} ({})",
                      code_points_filepath.string(), e.what());
        return;
    }
    if (!mapping.is_object()) {
        Logger::error("Invalid Lucide codepoints file: expected an object");
        return;
    }
    std::vector<std::string> names;
    names.reserve(mapping.size());
    std::vector<unsigned> code_points;
    code_points.reserve(mapping.size());
    try {
        for (auto& [name, value] : mapping.items()) {
            names.push_back(name);
            code_points.push_back(value.get<unsigned>());
        }
    } catch (std::exception const& e) {
        Logger::error("Invalid Lucide codepoint mapping: {}", e.what());
        return;
    }
    // Load font into the shared ImGui atlas
    auto* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
        font_filepath.string().c_str());
    if (!font) {
        Logger::error("Failed to load Lucide font: {}", font_filepath.string());
        return;
    }
    // Register icon set
    auto icon_ids = icon_manager.add_font_icon_set(font, code_points);

    auto& ui_context = get_current_ui_context();
    auto& table = ui_context.lucide_icon_table;
    table.clear();
    table.reserve(names.size());
    assert(names.size() == icon_ids.size());
    for (std::size_t i = 0; i < names.size(); ++i)
        ui_context.lucide_icon_table.emplace(std::move(names[i]), icon_ids[i]);

    icon_manager.set_fallback_icon(
        icon_manager.get_icon(ui::lucide_icon_id("circle-question-mark")));
}
