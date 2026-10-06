#include "gui/appearance.h"

#include <AppKit/Appkit.h>

#include "gui/appearance_internal.h"

using namespace mosaic;

AppearanceType mosaic::get_system_appearance() {
    NSAppearance* appearance = NSApp.effectiveAppearance;
    bool is_light =
        appearance.name == NSAppearanceNameAqua ||
        appearance.name == NSAppearanceNameVibrantLight ||
        appearance.name == NSAppearanceNameAccessibilityHighContrastAqua ||
        appearance.name ==
            NSAppearanceNameAccessibilityHighContrastVibrantLight;
    return is_light ? AppearanceType::light : AppearanceType::dark;
}

static vml::float4 convert_color(NSColor* color) {
    CIColor* tmp = [[CIColor alloc] initWithColor:color];
    return {(float)tmp.red, (float)tmp.green, (float)tmp.blue,
            (float)tmp.alpha};
}

static mosaic::Style make_style_impl(AppearanceType type) {
#define COL(name) convert_color(NSColor.name)
    // clang-format off
    return {
        .vars = {
            .window_border = false,
            .child_window_border = false,
            .popup_border = true,
            .frame_border = true,
            .tab_border = true,
            .tab_bar_border = true,
            .window_rounding = 4,
            .child_window_rounding = 0,
            .popup_rounding = 0,
            .frame_rounding = 6,
            .tab_rounding = 0,
            .selectable_rounding = 5,
            .grab_min_size = 12,
            .grab_rounding = 6,
            .scrollbar_size = 10,
            .scrollbar_rounding = 10,
            .window_title_align = {0.5, 0.5},
        },
        .colors = {
            .text = COL(labelColor),
            .text_disabled = COL(secondaryLabelColor),
            .window_bg = COL(controlBackgroundColor),
            .child_window_bg = COL(controlBackgroundColor),
            .popup_bg = COL(controlBackgroundColor),
            .menubar_bg = COL(controlBackgroundColor),
            .control_frame = COL(controlColor),
            .control_frame_hovered = COL(controlColor),
            .control_frame_down = COL(selectedControlColor),
            .highlight_control_frame = COL(controlAccentColor),
            .highlight_control_frame_hovered = COL(controlAccentColor),
            .highlight_control_frame_down = COL(selectedControlColor),
            .check_mark = COL(controlAccentColor),
            .window_title_bar = COL(windowBackgroundColor),
            .active_window_title_bar = COL(windowBackgroundColor),
            .border = COL(separatorColor),
            .scrollbar = type == AppearanceType::light ?
                             vml::float4{0, 0, 0, 0.247} :
                             COL(controlColor),
            .separator = COL(separatorColor),
            .tab = {0, 0, 0, 0},
            .tab_hovered = COL(selectedControlColor),
            .tab_active = COL(selectedControlColor),
            .tab_unfocused = {0, 0, 0, 0},
            .tab_unfocused_active = {0, 0, 0, 0},
            .docking_preview = COL(selectedControlColor),
            .drag_drop_target = COL(controlAccentColor),
        }
    };
    // clang-format on
}

static NSAppearanceName to_native_type(AppearanceType type) {
    switch (type) {
    case AppearanceType::light:
        return NSAppearanceNameAqua;
    case AppearanceType::dark:
        return NSAppearanceNameDarkAqua;
    }
}

mosaic::Style mosaic::make_style(AppearanceType type) {
    __block mosaic::Style result = {type};
    NSAppearance* a = [NSAppearance appearanceNamed:to_native_type(type)];
    [a performAsCurrentDrawingAppearance:^() {
      result = make_style_impl(type);
    }];
    return result;
}
