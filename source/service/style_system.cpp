#include "service/style_system.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "application/directories.h"
#include "style/style.h"
#include "style/style_desc.h"
#include "style/style_internal.h"
#include "support/json_support.h"
#include "support/logger.h"

using namespace mosaic;

struct StyleSystem::Impl {
    std::filesystem::path style_path;
    StyleDesc style_desc;
    StyleType style_type;
    Style style_ref;
    StyleContext style_context;

    void load_style_into_desc(std::filesystem::path const& path);

    void apply_style();
};

void StyleSystem::Impl::load_style_into_desc(
    std::filesystem::path const& path) {
    style_path = std::move(path);
    std::filesystem::path resolved_path = [&] {
        if (style_path.is_relative()) return resource_directory() / style_path;
        return style_path;
    }();
    std::fstream file(resolved_path);
    if (!file) {
        Logger::error("Failed to open style file: {}", resolved_path);
        return;
    }
    nlohmann::json object;
    try {
        file >> object;
        style_desc = object.get<StyleDesc>();
    } catch (nlohmann::json::exception const& e) {
        Logger::error("Failed to parse style file: {} ({})",
                      style_path.string(), e.what());
    }
}

void StyleSystem::Impl::apply_style() {
    assert(
        style_context.stacks_empty() &&
        "Must only do this when style mod stack is empty. Otherwise pops will override the modifications here.");
    style_ref = StyleContext::make_style(style_desc, style_type);
    style_context.m_style = style_ref;
    apply_to_imgui(style_ref);
}

StyleSystem::StyleSystem()
    : m_impl(std::make_unique<Impl>()) {}

StyleSystem::~StyleSystem() = default;

void StyleSystem::init() {
    if (m_impl->style_path.empty())
        m_impl->style_path = "style/default_style.json";
    m_impl->style_type = get_current_system_style_type();
    m_impl->load_style_into_desc(m_impl->style_path);
    m_impl->apply_style();
}

void StyleSystem::update() {
    StyleType const active_system_style_type = get_current_system_style_type();
    if (m_impl->style_type == active_system_style_type) return;
    set_style_type(active_system_style_type);
}

void StyleSystem::load_style(std::filesystem::path path) {
    m_impl->load_style_into_desc(path);
    m_impl->apply_style();
}

void StyleSystem::set_style_type(StyleType type) {
    m_impl->style_type = type;
    m_impl->apply_style();
}

void StyleSystem::set_style(StyleDesc style_desc) {
    m_impl->style_desc = style_desc;
    m_impl->apply_style();
}

void StyleSystem::set_style(StyleDesc style_desc, StyleType style_type) {
    m_impl->style_desc = style_desc;
    m_impl->style_type = style_type;
    m_impl->apply_style();
}

nlohmann::json StyleSystem::save_config() const {
    auto object = nlohmann::json::object();
    object["active_style"] = style_path();
    return object;
}

void StyleSystem::load_config(nlohmann::json const& root) {
    m_impl->style_path = json_find<std::filesystem::path>(root, "active_style")
                             .value_or(std::filesystem::path());
}

std::filesystem::path const& StyleSystem::style_path() const {
    return m_impl->style_path;
}

StyleDesc const& StyleSystem::style_desc() const { return m_impl->style_desc; }

StyleType StyleSystem::style_type() const { return m_impl->style_type; }

Style const& StyleSystem::style_ref() const { return m_impl->style_ref; }

StyleContext& StyleSystem::get_context() { return m_impl->style_context; }