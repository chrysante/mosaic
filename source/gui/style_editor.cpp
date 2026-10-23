#include "gui/style_editor.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <span>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <vml/vml.hpp>

#include "gui/font.h"
#include "gui/layout.h"

using namespace mosaic;

// =============================================================================
// Color types
// =============================================================================
struct StyleColor;

struct ModulatedColor {
    StyleColor const* source = nullptr;
    float saturation_factor = 1;
    float value_factor = 1;
    float hue_shift = 0;

    vml::float4 eval() const;
};

struct StyleColor {
    enum class Type { mod_color, raw_color };

    vml::float4 eval() const;

    std::string name;
    Type type;
    ModulatedColor mod_color;
    vml::float4 raw_color = {};
};

vml::float4 ModulatedColor::eval() const {
    if (!source) return {};
    vml::float4 hsva = vml::rgba_to_hsva(source->eval());
    hsva.hue = +std::fmod(hsva.hue + hue_shift, 1.0f);
    hsva.saturation *= saturation_factor;
    hsva.value *= value_factor;
    return vml::clamp(vml::hsva_to_rgba(hsva), {0.0f, 0.0f, 0.0f, 0.0f},
                      {1.0f, 1.0f, 1.0f, 1.0f});
}

vml::float4 StyleColor::eval() const {
    switch (type) {
    case Type::mod_color:
        return mod_color.eval();
    case Type::raw_color:
        return raw_color;
    }
}

// =============================================================================
// Color editors
// =============================================================================

static void
draw_color_edit_mod(StyleColor& color,
                    utl::function_view<void(StyleColor const*&)> source_combo) {
    auto& mod_color = color.mod_color;
    char const* preview =
        mod_color.source ? mod_color.source->name.c_str() : "(None)";
    ui::item("Source", [&] {
        if (ImGui::BeginCombo("##source", preview)) {
            source_combo(mod_color.source);
            ImGui::EndCombo();
        }
    });
    ui::item("Hue Shift", [&] {
        ImGui::SliderFloat("##hue-shift", &mod_color.hue_shift, -1.0f, 1.0f,
                           "%.3f");
    });
    ui::item("Saturation", [&] {
        ImGui::SliderFloat("##saturation", &mod_color.saturation_factor, 0.0f,
                           2.0f, "%.3f");
    });
    ui::item("Value", [&] {
        ImGui::SliderFloat("##value", &mod_color.value_factor, 0.0f, 2.0f,
                           "%.3f");
    });
}

static void draw_color_edit_raw(StyleColor& color) {
    ui::item("", [&] {
        auto flags =
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;
        ImGui::ColorEdit4("##raw", color.raw_color.data(), flags);
    });
}

namespace {
struct StyleColorEditOptions { bool edit_name = true; };
} // namespace

static void
draw_color_edit(int id, StyleColor& color,
                utl::function_view<void(StyleColor const*&)> source_combo,
                StyleColorEditOptions options = {}) {
    ui::section(color.name.c_str());
    ImGui::PushID(id);
    // Shared properties.
    ui::item("Preview", [&] {
        ImGui::ColorButton("##preview", color.eval(),
                           ImGuiColorEditFlags_AlphaPreviewHalf,
                           {ImGui::CalcItemWidth(), 24.0f});
    });
    if (options.edit_name)
        ui::item("Name", [&] { ImGui::InputText("##name", &color.name); });
    int type = static_cast<int>(color.type);
    char const* types[] = {"Modulated", "Raw"};
    ui::item("Type", [&] {
        if (ImGui::Combo("##type", &type, types, IM_ARRAYSIZE(types)))
            color.type = static_cast<StyleColor::Type>(type);
    });
    switch (color.type) {
    case StyleColor::Type::mod_color:
        draw_color_edit_mod(color, source_combo);
        break;
    case StyleColor::Type::raw_color:
        draw_color_edit_raw(color);
        break;
    }
    ImGui::PopID();
}

// =============================================================================
// Style editor infrastructure
// =============================================================================

struct OutputFields {};

struct StyleEditData {
    StyleEditData() {
#define MOSAIC_STYLE_COLOR(color_name, category)                               \
    output_colors.push_back({.name = #color_name});
#include <mosaic/gui/style.def>
    }

    std::string name;

    std::vector<std::unique_ptr<StyleColor>> primary_colors;
    std::vector<std::unique_ptr<StyleColor>> secondary_colors;
    std::vector<StyleColor> output_colors;
};

struct StyleEditor::Impl { std::unique_ptr<StyleEditData> style, new_style; };

StyleEditor::StyleEditor()
    : m_impl(std::make_unique<Impl>()) {}

StyleEditor::~StyleEditor() = default;

void StyleEditor::new_style_modal() {
    ImGui::SetNextWindowSize({500, 400});
    auto flags = ImGuiWindowFlags_NoResize;
    if (!ImGui::BeginPopupModal("New Style", nullptr, flags)) return;
    assert(m_impl->new_style);
    ui::begin_properties("new-style");
    ui::item(
        "Name", [&] { ImGui::InputText("##name", &m_impl->new_style->name); },
        [&] {
            ImGui::BeginGroup();
            if (ImGui::Button("Cancel")) {
                m_impl->new_style.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Create")) {
                m_impl->style = std::move(m_impl->new_style);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndGroup();
        });
    ui::end_properties();
    ImGui::EndPopup();
}

void StyleEditor::empty_editor() {
    if (ImGui::Button("New Style...")) {
        m_impl->new_style = std::make_unique<StyleEditData>();
        ImGui::OpenPopup("New Style");
    }
    new_style_modal();
}

static void header(char const* title,
                   std::vector<std::unique_ptr<StyleColor>>& colors) {
    auto cp = ImGui::GetCursorScreenPos();

    float width = ImGui::GetContentRegionAvail().x;
    IconData icon = ui::icon_data("plus-squared");
    bool pressed = ui::with_font(icon.font, [&] {
        ImVec2 size = {60, ImGui::GetFontSize() +
                               2 * ImGui::GetStyle().FramePadding.y};
        ImVec2 pos = {cp.x + width - size.x, cp.y};
        ImGui::SetCursorScreenPos(pos);
        return ImGui::ButtonEx(icon.utf8_rep.data(), size);
    });
    if (pressed) ImGui::OpenPopup("##new-color-popup");
    ImGui::SetNextWindowSize({500, 400});
    auto flags = ImGuiWindowFlags_NoResize;
    if (ImGui::BeginPopupModal("##new-color-popup", nullptr, flags)) {
        ui::begin_properties("new-color-props");
        static char buf[256];
        ui::item(
            "Name", [&] { ImGui::InputText("##name", buf, sizeof buf); },
            [&] {
                ImGui::BeginGroup();
                if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (ImGui::Button("Create")) {
                    colors.push_back(
                        std::make_unique<StyleColor>(StyleColor{.name = buf}));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndGroup();
            });
        ui::end_properties();
        ImGui::EndPopup();
    }

    ImGui::SetCursorScreenPos(cp);
    ImGui::Text("%s", title);
}

static void primary_colors(StyleEditData& style) {
    ui::begin_properties("primary-colors");

    auto source_combo = [&](StyleColor const*&) {};
    for (int id = 0; auto& color : style.primary_colors)
        draw_color_edit(id++, *color, source_combo);
    ui::end_properties();
}

static void
source_combo_list(StyleColor const*& source,
                  std::span<std::unique_ptr<StyleColor> const> colors) {
    for (auto& color : colors) {
        bool is_selected = color.get() == source;
        if (ImGui::Selectable(color->name.c_str(), is_selected))
            source = color.get();
        if (is_selected) ImGui::SetItemDefaultFocus();
    }
}

static void secondary_colors(StyleEditData& style) {
    ui::begin_properties("secondary-colors");
    auto source_combo = [&](StyleColor const*& source) {
        source_combo_list(source, style.primary_colors);
    };
    for (int id = 0; auto& color : style.secondary_colors)
        draw_color_edit(id++, *color, source_combo);
    ui::end_properties();
}

static void output_colors(StyleEditData& style) {
    ui::begin_properties("output-colors");
    auto source_combo = [&](StyleColor const*& source) {
        source_combo_list(source, style.primary_colors);
        source_combo_list(source, style.secondary_colors);
    };
    for (int id = 0; auto& color : style.output_colors)
        draw_color_edit(id++, color, source_combo, {.edit_name = false});
    ui::end_properties();
}

void StyleEditor::draw() {
    if (!m_impl->style) {
        empty_editor();
        return;
    }
    auto& style = *m_impl->style;
    ui::ColumnLayout colums[] = {
        {.header = [&] { header("Primary Colors", style.primary_colors); },
         .content = [&] { primary_colors(style); },
         .min_width = 200.0f},
        {.header = [&] { header("Secondary Colors", style.secondary_colors); },
         .content = [&] { secondary_colors(style); },
         .min_width = 200.0f},
        {.header = "Output Colors",
         .content = [&] { output_colors(style); },
         .min_width = 200.0f}};
    ui::columns("style_editor_columns", colums);
}