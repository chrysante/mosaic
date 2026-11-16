#pragma once

#include <filesystem>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/gui/style.h>
#include <mosaic/gui/style_desc.h>

namespace mosaic {

class StyleSystem {
public:
    void init();

    void update();

    void load_style(std::filesystem::path path);

    void set_style(StyleDesc style_desc);

    void set_style(StyleDesc style_desc, StyleType type);

    void set_style_type(StyleType type);

    nlohmann::json save_config() const;

    void load_config(nlohmann::json const& root);

    std::filesystem::path const& active_style_path() const {
        return m_active_style_path;
    }

    StyleDesc const& active_style_desc() const { return m_active_style_desc; }

    StyleType active_style_type() const { return m_active_style_type; }

    /// The active style, not modified by stack pushes
    Style_ const& active_style_ref() const { return m_active_style_ref; }

    /// The active style including current modifications
    StyleContext& active_style_contex() { return m_active_style_context; }

private:
    void apply_style(StyleType type, StyleDesc const& style_desc);

    std::filesystem::path m_active_style_path;
    StyleDesc m_active_style_desc;
    StyleType m_active_style_type;
    Style_ m_active_style_ref;
    StyleContext m_active_style_context;
};

} // namespace mosaic