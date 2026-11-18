#pragma once

#include <filesystem>
#include <memory>

#include <nlohmann/json_fwd.hpp>

#include <mosaic/style/style.h>
#include <mosaic/style/style_desc.h>

namespace mosaic {

class StyleContext;

class StyleSystem {
public:
    StyleSystem();
    ~StyleSystem();

    void init();

    void update();

    void load_style(std::filesystem::path path);

    void set_style(StyleDesc style_desc);

    void set_style(StyleDesc style_desc, StyleType type);

    void set_style_type(StyleType type);

    nlohmann::json save_config() const;

    void load_config(nlohmann::json const& root);

    std::filesystem::path const& style_path() const;

    /// The descriptor of the currently active style
    StyleDesc const& style_desc() const;

    /// The active style type
    StyleType style_type() const;

    /// The active style, not modified by stack pushes
    Style const& style_ref() const;

    StyleContext& get_context();

private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
};

} // namespace mosaic