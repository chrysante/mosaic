#pragma once

#include <vml/vml.hpp>

namespace mosaic {

///
enum struct AppearanceType { light, dark };

/// UI variables used in the application
struct StyleVars {
    bool window_border;
    bool child_window_border;
    bool popup_border;
    bool frame_border;
    bool tab_border;
    bool tab_bar_border;
    float window_rounding;
    float child_window_rounding;
    float popup_rounding;
    float frame_rounding;
    float tab_rounding;
    float selectable_rounding;
    float grab_min_size;
    float grab_rounding;
    float scrollbar_size;
    float scrollbar_rounding;
    vml::float2 window_title_align;
};

/// UI colors used in the application
struct StyleColors {
    vml::float4 text;
    vml::float4 text_disabled;
    vml::float4 window_bg;
    vml::float4 child_window_bg;
    vml::float4 popup_bg;
    vml::float4 menubar_bg;
    vml::float4 control_frame;
    vml::float4 control_frame_hovered;
    vml::float4 control_frame_down;
    vml::float4 highlight_control_frame;
    vml::float4 highlight_control_frame_hovered;
    vml::float4 highlight_control_frame_down;
    vml::float4 check_mark;
    vml::float4 window_title_bar;
    vml::float4 active_window_title_bar;
    vml::float4 border;
    vml::float4 scrollbar;
    vml::float4 separator;
    vml::float4 tab;
    vml::float4 tab_hovered;
    vml::float4 tab_active;
    vml::float4 tab_unfocused;
    vml::float4 tab_unfocused_active;
    vml::float4 docking_preview;
    vml::float4 drag_drop_target;
};

/// Intermediate data structure that is derived from system colors or hard coded
/// and then used to set ImGui colors and style vars
struct Style {
    AppearanceType type;
    StyleVars vars;
    StyleColors colors;
};

///
Style get_global_style();

} // namespace mosaic
