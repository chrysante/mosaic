#include "style/style_editor.h"

#include <filesystem>
#include <fstream>
#include <memory>

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include "application/resource_util.h"
#include "gui/containers.h"
#include "gui/property_editor.h"
#include "gui/toolbar.h"
#include "gui/view_support.h"
#include "gui/widgets.h"
#include "service/style_system.h"
#include "style/style.h"
#include "style/style_class.h"
#include "style/style_desc.h"
#include "support/logger.h"
#include "support/memory.h"

using namespace mosaic;
using namespace ui;

namespace {

struct DeferQueue {
    void defer(std::function<void()> f) {
        dispatch_items.push_back(std::move(f));
    }

    void pump() {
        for (auto& f : dispatch_items)
            f();
        dispatch_items.clear();
    }

    std::vector<std::function<void()>> dispatch_items;
};

} // namespace

struct mosaic::internal::StyleEditorImpl {
    StyleEditorImpl(StyleSystem& style_system)
        : style_system(style_system)
        , style_desc(style_system.style_desc())
        , style_type(style_system.style_type()) {
        init();
    }

    void init();

    void save();
    void save_as();
    void apply() {
        defer_queue.defer(
            [this] { style_system.set_style(style_desc, style_type); });
    }

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

    Property<vml::float3> bind_modifier(ColorModifier& mod) {
        return computed(
            [&mod] -> vml::float3 {
                return {mod.saturation_scale, mod.value_scale, mod.alpha_scale};
            },
            [&](vml::float3 new_value) {
                mod = {new_value.x, new_value.y, new_value.z};
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
        return property(
            name, hstack({
                      slider<vml::float3>(bind_modifier(light), 0.0f, 1.0f),
                      slider<vml::float3>(bind_modifier(dark), 0.0f, 1.0f),
                  }) | hfill);
    }

    Owner<View> field(std::string name, ColorDesc& light, ColorDesc& dark) {
        return property(std::move(name),
                        hstack({color_edit(bind_color(light)),
                                color_edit(bind_color(dark))}) |
                            hfill);
    }

    Owner<View> field(std::string name, ControlColorDesc& light,
                      ControlColorDesc& dark) {
        auto two_color = [this](auto& light, auto& dark) {
            return hstack({color_edit(bind_color(light)),
                           color_edit(bind_color(dark))}) |
                   hfill;
        };
        return property(
            std::move(name),
            vstack({property("Background",
                             two_color(light.background, dark.background)),
                    property("Background Hovered",
                             two_color(light.background_hovered,
                                       dark.background_hovered)),
                    property("Background Active",
                             two_color(light.background_active,
                                       dark.background_active)),
                    property("Border", two_color(light.border, dark.border)),
                    property("Text", two_color(light.text, dark.text))}) |
                hfill);
    }

    Owner<View> editor_view() {
        auto& style_class = StyleClass::get();
        ViewContainer<> base_colors, control_colors;
        for (unsigned index = 0; index < style_desc.colors.size(); ++index) {
            std::string name =
                style_class.get_metadata(StyleColorId{index}).name;
            base_colors.push_back(field(name, style_desc.colors[index][0],
                                        style_desc.colors[index][1]));
        }
        for (unsigned index = 0; index < style_desc.control_colors.size();
             ++index) {
            std::string name =
                style_class.get_metadata(StyleControlColorId{index}).name;
            control_colors.push_back(
                field(name, style_desc.control_colors[index][0],
                      style_desc.control_colors[index][1]));
        }
        return vscroll({section("Base Colors", std::move(base_colors)),
                        section("Control Colors", std::move(control_colors))});
    }

    void update() { defer_queue.pump(); }

    StyleSystem& style_system;
    std::string open_file_path;
    StyleDesc style_desc;
    StyleType style_type;
    Owner<ui::View> view;
    DeferQueue defer_queue;
};

using mosaic::internal::StyleEditorImpl;

void StyleEditorImpl::init() {
    auto style_type_combo =
        combo(computed([&] { return (int)style_type; },
                       [&](int new_value) {
                           style_type = (StyleType)new_value;
                           apply();
                       }),
              computed([&] -> std::string {
                  return style_type == StyleType::light ? "Light" : "Dark";
              }),
              std::vector<std::string>{"Light", "Dark"}) |
        hfill_intrin;
    auto toolbar = ui::toolbar({button("Save", [this] { save(); }),
                                button("Save As...", [&] { save_as(); }),
                                spacer(), label(computed([&] {
                                    return style_system.style_path().string();
                                })),
                                spacer(), std::move(style_type_combo)}) |
                   fixed_height(38);
    ui::internal::set_toolbar_padding(toolbar.get(), {6.0f, 6.0f, 6.0f, 6.0f});
    view = vstack({std::move(toolbar), editor_view()}) | hfill | vfill;
}

void StyleEditorImpl::save() {
    save_style(style_system.style_path(), style_desc);
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
    file << j.dump(4);
    Logger::info("Successfully saved style file");
}

StyleEditor::StyleEditor(StyleSystem& style_system)
    : m_impl(std::make_unique<internal::StyleEditorImpl>(style_system)) {}

StyleEditor::~StyleEditor() = default;

void StyleEditor::draw() {
    m_impl->view->display(
        {ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail()});
    m_impl->update();
}