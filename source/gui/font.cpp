#include "gui/font.h"

#include <algorithm>
#include <imgui.h>

#include "gui/ui_context.h"
#include "service/font_manager.h"

using namespace mosaic;

FontContext ui::get_current_font() {
    auto& font_stack = get_current_ui_context().font_stack;
    return font_stack.top();
}

static void push_font_impl(FontDesc desc, float size_absolute) {
    auto& ui_context = get_current_ui_context();
    auto& font_manager = ui_context.font_manager;
    auto& font_stack = ui_context.font_stack;
    auto* font = font_manager.get_font(desc);
    font_stack.push({.desc = desc, .size = size_absolute, .font = font});
    ImGui::PushFont(font, size_absolute);
}

void ui::push_font(FontModifier mod) {
    auto& ui_context = get_current_ui_context();
    auto& font_stack = ui_context.font_stack;
    FontContext current = font_stack.top();

    FontDesc desc = current.desc;
    float size = current.size;

    // Weight
    if (mod.weight_override) desc.weight = *mod.weight_override;
    int new_weight = (int)desc.weight + mod.weight_offset;
    desc.weight = (FontWeight)std::clamp(new_weight, 0, (int)FontWeight::black);

    // Style
    if (mod.style_override) desc.style = *mod.style_override;

    // Size
    if (mod.size_override > 0.0f)
        size = mod.size_override * ImGui::GetStyle().FontSizeBase;
    size += mod.size_add;
    size *= mod.size_scale;

    push_font_impl(desc, size);
}

void ui::push_font(FontDesc desc, float size) {
    auto& style = ImGui::GetStyle();
    push_font_impl(desc, size * style.FontSizeBase);
}

void ui::pop_font() {
    ImGui::PopFont();
    auto& font_stack = get_current_ui_context().font_stack;
    font_stack.pop();
}

IconData ui::icon_data(std::string_view name) {
    auto& ui_context = get_current_ui_context();
    return ui_context.font_manager.get_icon(name);
}