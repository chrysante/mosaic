#include "service/style_system.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "application/directories.h"
#include "gui/style.h"
#include "gui/style_desc.h"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;

static void apply_style(StyleType type, StyleDesc const& style) {
    auto& g_style = get_global_style_write();
    g_style = compute_from_desc(style, type);
    apply_to_imgui(g_style);
}

void StyleSystem::init() {
    if (m_active_style_path.empty())
        m_active_style_path = "style/default_style.json";
    load_style(std::move(m_active_style_path));
}

void StyleSystem::load_style(std::filesystem::path path) {
    m_active_style_path = std::move(path);
    auto resolved_path = resource_directory() / m_active_style_path;
    std::fstream file(resolved_path);
    if (!file) {
        Logger::error("Failed to open style file: {}", resolved_path);
        return;
    }
    nlohmann::json object;
    try {
        file >> object;
        m_active_style = object.get<StyleDesc>();

    } catch (nlohmann::json::exception const& e) {
        Logger::error("Failed to parse style file: {} ({})",
                      m_active_style_path.string(), e.what());
        return;
    }
}

void StyleSystem::set_style_type(StyleType type) {
    m_active_style_type = type;
    apply_style(m_active_style_type, m_active_style);
}

void StyleSystem::set_style(StyleDesc style_desc) {
    m_active_style = style_desc;
    apply_style(m_active_style_type, m_active_style);
}

nlohmann::json StyleSystem::save_config() const {
    auto object = nlohmann::json::object();
    object["active_style"] = active_style_path();
    object["style_type"] = (int)m_active_style_type;
    return object;
}

void StyleSystem::load_config(nlohmann::json const& root) {
    m_active_style_path = json_find<std::filesystem::path>(root, "active_style")
                              .value_or(std::filesystem::path());
    m_active_style_type =
        (StyleType)json_find<int>(root, "style_type").value_or(1);
}