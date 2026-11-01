#pragma once

#include <filesystem>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/gui/style.h>
#include <mosaic/gui/style_desc.h>

namespace mosaic {

class StyleSystem {
public:
    void init();

    void load_style(std::filesystem::path path);

    void set_style(StyleDesc style_desc);

    void set_style_type(StyleType type);

    nlohmann::json save_config() const;

    void load_config(nlohmann::json const& root);

    std::filesystem::path const& active_style_path() const {
        return m_active_style_path;
    }

    StyleDesc const& active_style() const { return m_active_style; }

    StyleType active_style_type() const { return m_active_style_type; }

private:
    std::filesystem::path m_active_style_path;
    StyleDesc m_active_style;
    StyleType m_active_style_type;
};

} // namespace mosaic