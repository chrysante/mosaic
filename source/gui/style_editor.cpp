#include "gui/style_editor.h"

#include <filesystem>
#include <fstream>
#include <memory>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include "application/resource_util.h"
#include "gui/containers.h"
#include "gui/icon.h"
#include "gui/property_editor.h"
#include "gui/style.h"
#include "gui/style_desc.h"
#include "gui/toolbar.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "nlohmann/json_fwd.hpp"
#include "service/style_system.h"
#include "support/logger.h"
#include "support/memory.h"

using namespace mosaic;
using namespace ui;

struct mosaic::internal::StyleEditorImpl {
    StyleEditorImpl(StyleSystem& style_system)
        : style_system(style_system)
        , style_desc(style_system.active_style())
        , style_type(style_system.active_style_type()) {
        init();
    }

    void init();

    void save();
    void save_as();
    void apply() { style_system.set_style(style_desc); }

    void save_style(std::filesystem::path path, StyleDesc style);

    Property<vml::float4> bind_color(ColorDesc& desc) {
        return computed(
            [&desc] -> vml::float4 {
                return ImGui::ColorConvertU32ToFloat4(desc.value);
            },
            [&](vml::float4 new_value) {
                desc.value = ImGui::ColorConvertFloat4ToU32(new_value);
                apply();
            });
    }

    Property<float> bind_float(float& value) {
        return computed([&value] { return value; },
                        [&](float new_value) {
                            value = new_value;
                            apply();
                        });
    }

    Owner<View> mod_edit(std::string name, ColorModifier& light,
                         ColorModifier& dark) {
        auto two_slider = [this](auto& light, auto& dark) {
            return hstack({slider<float>(bind_float(light), 0.0f, 4.0f),
                           slider<float>(bind_float(dark), 0.0f, 4.0f)}) |
                   hfill;
        };
        return vstack(
                   {property(name + ".Saturation",
                             two_slider(light.saturation_scale,
                                        dark.saturation_scale)),
                    property(name + ".Value",
                             two_slider(light.value_scale, dark.value_scale)),
                    property(name + ".Alpha", two_slider(light.alpha_scale,
                                                         dark.alpha_scale))}) |
               hfill;
    }

    Owner<View> field(std::string name, ControlColorDesc& light,
                      ControlColorDesc& dark) {
        auto two_color = hstack({color_edit(bind_color(light.base)),
                                 color_edit(bind_color(dark.base))}) |
                         hfill;
        return property(std::move(name),
                        vstack({
                            property("Base", std::move(two_color)),
                            mod_edit("Hovered", light.hovered, dark.hovered),
                            mod_edit("Active", light.active, dark.active),
                        }) | hfill);
    }

    Owner<View> colors(StyleDesc::Colors* colors) {
        return section({field("Control", colors[0].control, colors[1].control),
                        field("Control Accent", colors[0].control_accent,
                              colors[1].control_accent)});
    }

    StyleSystem& style_system;
    std::string open_file_path;
    StyleDesc style_desc;
    StyleType style_type;
    Owner<ui::View> view;
};

using mosaic::internal::StyleEditorImpl;

void StyleEditorImpl::init() {
    auto style_type_combo =
        combo(computed([&] { return (int)style_type; },
                       [&](int new_value) {
                           style_type = (StyleType)new_value;
                           style_system.set_style_type(style_type);
                       }),
              computed([&] -> std::string {
                  return style_type == StyleType::light ? "Light" : "Dark";
              }),
              std::vector<std::string>{"Light", "Dark"}) |
        hfill_intrin;
    auto toolbar =
        ui::toolbar({button("Save", [this] { save(); }),
                     button("Save As...", [&] { save_as(); }), spacer(),
                     label(computed([&] {
                         return style_system.active_style_path().string();
                     })),
                     spacer(), std::move(style_type_combo)}) |
        fixed_height(38);
    ui::internal::set_toolbar_padding(toolbar.get(), {6.0f, 6.0f, 6.0f, 6.0f});

    auto editor_view = vscroll({colors(style_desc.color)});
    view = vstack({std::move(toolbar), std::move(editor_view)}) | hfill | vfill;
}

void StyleEditorImpl::save() {
    save_style(style_system.active_style_path(), style_desc);
}

void StyleEditorImpl::save_as() {
    show_save_panel([&, style_desc = style_desc](std::filesystem::path path) {
        save_style(path, style_desc);
    });
}

void StyleEditorImpl::save_style(std::filesystem::path path, StyleDesc style) {
    std::ofstream file(path);
    if (!file) {
        Logger::error("Failed to save style file");
        return;
    }
    nlohmann::json j = style;
    file << j;
    Logger::info("Successfully saved style file");
}

StyleEditor::StyleEditor(StyleSystem& style_system)
    : m_impl(std::make_unique<internal::StyleEditorImpl>(style_system)) {}

StyleEditor::~StyleEditor() = default;

void StyleEditor::draw() {
    m_impl->view->display(
        {ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail()});
}