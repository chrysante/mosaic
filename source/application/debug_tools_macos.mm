#include "application/debug_tools.h"

#ifndef NDEBUG

#import <Cocoa/Cocoa.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <utl/strcat.hpp>

#include "application/window.h"
#include "application/window_impl_macos.h"

using namespace mosaic;

static const char* ns_window_level_name(CGFloat level) {
    if (level == NSNormalWindowLevel) return "Normal";
    if (level == NSFloatingWindowLevel) return "Floating";
    if (level == NSModalPanelWindowLevel) return "Modal Panel";
    if (level == NSMainMenuWindowLevel) return "Main Menu";
    if (level == NSStatusWindowLevel) return "Status";
    if (level == NSPopUpMenuWindowLevel) return "Pop-up Menu";
    if (level == NSScreenSaverWindowLevel) return "Screen Saver";
    return "Custom";
}

static const char* ns_window_appearance_name(NSAppearance* appearance) {
    if (!appearance) return "nil";
    NSString* name = appearance.name;
    if ([name isEqualToString:NSAppearanceNameAqua]) return "Aqua";
    if ([name isEqualToString:NSAppearanceNameDarkAqua]) return "Dark Aqua";
    if ([name isEqualToString:NSAppearanceNameVibrantLight])
        return "Vibrant Light";
    if ([name isEqualToString:NSAppearanceNameVibrantDark])
        return "Vibrant Dark";
    return name.UTF8String;
}

static bool is_only_imgui_window_in_viewport(ImGuiWindow* window) {
    ImGuiContext& g = *GImGui;
    for (ImGuiWindow* other : g.Windows) {
        if (other == window) continue;
        if (other->Viewport == window->Viewport) return false;
    }
    return true;
}

static void draw_native_window_style_inspector() {
    ImGuiWindow* imgui_window = ImGui::GetCurrentWindow();
    const bool only_window = is_only_imgui_window_in_viewport(imgui_window);
    auto* viewport = imgui_window->Viewport;
    auto* window = viewport ? (Window*)viewport->PlatformHandle : nullptr;
    NSWindow* native_window =
        window ? GET_WINDOW_IMPL(*window).native_window : nil;
    ImGui::SeparatorText("NSWindow Inspector");
    if (!only_window) {
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.1f, 1.0f),
                           "This viewport contains other ImGui windows.");
        ImGui::TextUnformatted(
            "Move/detach this window so it owns its viewport.");
        return;
    }
    if (!native_window) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                           "No native NSWindow.");
        return;
    }
    ImGui::Text("Viewport: %08X", viewport->ID);
    ImGui::Text("NSWindow: %p", native_window);
    ImGui::SeparatorText("Window Level");
    {
        CGFloat level = native_window.level;
        const char* level_name = ns_window_level_name(level);
        if (ImGui::BeginCombo("Level", level_name)) {
            struct Level {
                const char* name;
                CGFloat value;
            };
            static const Level levels[] = {
                {"Normal", NSNormalWindowLevel},
                {"Floating", NSFloatingWindowLevel},
                {"Modal Panel", NSModalPanelWindowLevel},
                {"Main Menu", NSMainMenuWindowLevel},
                {"Status", NSStatusWindowLevel},
                {"Pop-up Menu", NSPopUpMenuWindowLevel},
                {"Screen Saver", NSScreenSaverWindowLevel},
            };
            for (const Level& item : levels) {
                const bool selected = level == item.value;
                if (ImGui::Selectable(item.name, selected))
                    native_window.level = item.value;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Raw level: %.0f", level);
    }
    ImGui::SeparatorText("Style Mask");
    {
        NSUInteger mask = native_window.styleMask;
        struct Style {
            const char* name;
            NSWindowStyleMask flag;
        };
        static const Style styles[] = {
            {"Borderless", NSWindowStyleMaskBorderless},
            {"Titled", NSWindowStyleMaskTitled},
            {"Closable", NSWindowStyleMaskClosable},
            {"Miniaturizable", NSWindowStyleMaskMiniaturizable},
            {"Resizable", NSWindowStyleMaskResizable},
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
            {"Unified Title And Toolbar",
             NSWindowStyleMaskUnifiedTitleAndToolbar},
#endif
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
            {"Full Size Content View", NSWindowStyleMaskFullSizeContentView},
#endif
        };
        for (auto [name, flag] : styles) {
            bool enabled = (mask & flag) != 0;
            if (ImGui::Checkbox(utl::strcat(name, "##style").c_str(),
                                &enabled)) {
                if (enabled) mask |= flag;
                else mask &= ~flag;
                native_window.styleMask = mask;
            }
        }
        ImGui::Text("Raw mask: 0x%llX", (unsigned long long)mask);
    }
    ImGui::SeparatorText("Title Bar");
    {
        bool transparent = native_window.titlebarAppearsTransparent;
        if (ImGui::Checkbox("Titlebar appears transparent", &transparent))
            native_window.titlebarAppearsTransparent = transparent;
        bool movable = native_window.movableByWindowBackground;
        if (ImGui::Checkbox("Movable by window background", &movable))
            native_window.movableByWindowBackground = movable;
        bool full_size = (native_window.styleMask &
                          NSWindowStyleMaskFullSizeContentView) != 0;
        if (ImGui::Checkbox("Full-size content view", &full_size)) {
            NSUInteger mask = native_window.styleMask;
            if (full_size) mask |= NSWindowStyleMaskFullSizeContentView;
            else mask &= ~NSWindowStyleMaskFullSizeContentView;
            native_window.styleMask = mask;
        }
        bool title_hidden =
            native_window.titleVisibility == NSWindowTitleHidden;
        if (ImGui::Checkbox("Hide title", &title_hidden)) {
            native_window.titleVisibility =
                title_hidden ? NSWindowTitleHidden : NSWindowTitleVisible;
        }
        char title[256];
        NSString* ns_title = native_window.title ?: @"";
        snprintf(title, sizeof(title), "%s", ns_title.UTF8String ?: "");
        if (ImGui::InputText("Title", title, sizeof(title)))
            native_window.title = [NSString stringWithUTF8String:title];
    }
    ImGui::SeparatorText("Behavior");
    {
        bool hides_on_deactivate = native_window.hidesOnDeactivate;
        if (ImGui::Checkbox("Hides on deactivate", &hides_on_deactivate))
            native_window.hidesOnDeactivate = hides_on_deactivate;
        bool ignores_mouse = native_window.ignoresMouseEvents;
        if (ImGui::Checkbox("Ignores mouse events", &ignores_mouse))
            native_window.ignoresMouseEvents = ignores_mouse;
        bool accepts_mouse_moved = native_window.acceptsMouseMovedEvents;
        if (ImGui::Checkbox("Accepts mouse moved events", &accepts_mouse_moved))
            native_window.acceptsMouseMovedEvents = accepts_mouse_moved;
        bool released_when_closed = native_window.releasedWhenClosed;
        if (ImGui::Checkbox("Released when closed", &released_when_closed))
            native_window.releasedWhenClosed = released_when_closed;
        bool movable = native_window.movable;
        if (ImGui::Checkbox("Movable", &movable))
            native_window.movable = movable;
        bool resizable =
            (native_window.styleMask & NSWindowStyleMaskResizable) != 0;
        if (ImGui::Checkbox("Resizable", &resizable)) {
            NSUInteger mask = native_window.styleMask;
            if (resizable) mask |= NSWindowStyleMaskResizable;
            else mask &= ~NSWindowStyleMaskResizable;
            native_window.styleMask = mask;
        }
        bool miniaturizable =
            (native_window.styleMask & NSWindowStyleMaskMiniaturizable) != 0;
        if (ImGui::Checkbox("Miniaturizable", &miniaturizable)) {
            NSUInteger mask = native_window.styleMask;
            if (miniaturizable) mask |= NSWindowStyleMaskMiniaturizable;
            else mask &= ~NSWindowStyleMaskMiniaturizable;
            native_window.styleMask = mask;
        }
    }
    ImGui::SeparatorText("Appearance");
    {
        bool opaque = native_window.opaque;
        if (ImGui::Checkbox("Opaque", &opaque)) native_window.opaque = opaque;
        bool shadow = native_window.hasShadow;
        if (ImGui::Checkbox("Has shadow", &shadow))
            native_window.hasShadow = shadow;
        float alpha = native_window.alphaValue;
        if (ImGui::SliderFloat("Alpha", &alpha, 0.0f, 1.0f))
            native_window.alphaValue = alpha;
        NSColor* color = native_window.backgroundColor;
        if (color) {
            NSColor* rgb =
                [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
            if (rgb) {
                CGFloat r, g, b, a;
                [rgb getRed:&r green:&g blue:&b alpha:&a];
                float color4[4] = {(float)r, (float)g, (float)b, (float)a};
                if (ImGui::ColorEdit4("Background color", color4)) {
                    native_window.backgroundColor =
                        [NSColor colorWithSRGBRed:color4[0]
                                            green:color4[1]
                                             blue:color4[2]
                                            alpha:color4[3]];
                }
            }
        }
        NSAppearance* appearance = native_window.appearance;
        if (ImGui::BeginCombo("Appearance",
                              ns_window_appearance_name(appearance))) {
            struct Appearance {
                const char* name;
                NSString* value;
            };
            static const Appearance appearances[] = {
                {"System", nil},
                {"Aqua", NSAppearanceNameAqua},
                {"Dark Aqua", NSAppearanceNameDarkAqua},
                {"Vibrant Light", NSAppearanceNameVibrantLight},
                {"Vibrant Dark", NSAppearanceNameVibrantDark},
            };
            for (const Appearance& item : appearances) {
                const bool selected =
                    item.value == nil
                        ? appearance == nil
                        : [appearance.name isEqualToString:item.value];
                if (ImGui::Selectable(item.name, selected)) {
                    if (item.value)
                        native_window.appearance =
                            [NSAppearance appearanceNamed:item.value];
                    else native_window.appearance = nil;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SeparatorText("Collection Behavior");
    {
        NSWindowCollectionBehavior behavior = native_window.collectionBehavior;
        struct CollectionBehavior {
            const char* name;
            NSWindowCollectionBehavior flag;
        };
        static const CollectionBehavior behaviors[] = {
            {"Managed", NSWindowCollectionBehaviorManaged},
            {"Transient", NSWindowCollectionBehaviorTransient},
            {"Move to active space",
             NSWindowCollectionBehaviorMoveToActiveSpace},
            {"Full screen primary",
             NSWindowCollectionBehaviorFullScreenPrimary},
            {"Full screen auxiliary",
             NSWindowCollectionBehaviorFullScreenAuxiliary},
            {"Can join all spaces", NSWindowCollectionBehaviorCanJoinAllSpaces},
            {"Stationary", NSWindowCollectionBehaviorStationary},
            {"Ignores cycle", NSWindowCollectionBehaviorIgnoresCycle},
        };
        for (const CollectionBehavior& item : behaviors) {
            bool enabled = (behavior & item.flag) != 0;
            if (ImGui::Checkbox(item.name, &enabled)) {
                if (enabled) behavior |= item.flag;
                else behavior &= ~item.flag;
                native_window.collectionBehavior = behavior;
            }
        }
        ImGui::Text("Raw behavior: 0x%llX", (unsigned long long)behavior);
    }
    ImGui::SeparatorText("Quick Presets");
    if (ImGui::Button("Normal")) {
        native_window.level = NSNormalWindowLevel;
        native_window.styleMask =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
            NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
        native_window.titleVisibility = NSWindowTitleVisible;
        native_window.titlebarAppearsTransparent = NO;
        native_window.movableByWindowBackground = NO;
        native_window.hidesOnDeactivate = NO;
        native_window.hasShadow = YES;
        native_window.opaque = YES;
        native_window.alphaValue = 1.0;
        native_window.ignoresMouseEvents = NO;
        native_window.collectionBehavior = NSWindowCollectionBehaviorManaged;
    }
    ImGui::SameLine();
    if (ImGui::Button("Floating")) {
        native_window.level = NSFloatingWindowLevel;
        native_window.hidesOnDeactivate = NO;
        native_window.hasShadow = YES;
        native_window.alphaValue = 1.0;
        native_window.ignoresMouseEvents = NO;
    }
    ImGui::SameLine();
    if (ImGui::Button("Tooltip")) {
        native_window.level = NSFloatingWindowLevel;
        native_window.styleMask = NSWindowStyleMaskBorderless;
        native_window.titleVisibility = NSWindowTitleHidden;
        native_window.titlebarAppearsTransparent = YES;
        native_window.movableByWindowBackground = NO;
        native_window.hidesOnDeactivate = YES;
        native_window.hasShadow = YES;
        native_window.opaque = NO;
        native_window.alphaValue = 1.0;
        native_window.ignoresMouseEvents = YES;
        native_window.collectionBehavior =
            NSWindowCollectionBehaviorTransient |
            NSWindowCollectionBehaviorMoveToActiveSpace;
    }
    ImGui::SameLine();
    if (ImGui::Button("Menu")) {
        native_window.level = NSPopUpMenuWindowLevel;
        native_window.styleMask = NSWindowStyleMaskBorderless;
        native_window.titleVisibility = NSWindowTitleHidden;
        native_window.titlebarAppearsTransparent = YES;
        native_window.movableByWindowBackground = NO;
        native_window.hidesOnDeactivate = YES;
        native_window.hasShadow = YES;
        native_window.opaque = YES;
        native_window.alphaValue = 1.0;
        native_window.ignoresMouseEvents = NO;
        native_window.collectionBehavior =
            NSWindowCollectionBehaviorTransient |
            NSWindowCollectionBehaviorMoveToActiveSpace;
    }
    ImGui::SeparatorText("Native State");
    ImGui::Text("Visible:        %s", native_window.isVisible ? "yes" : "no");
    ImGui::Text("Key window:     %s", native_window.isKeyWindow ? "yes" : "no");
    ImGui::Text("Main window:    %s",
                native_window.isMainWindow ? "yes" : "no");
    ImGui::Text("On active space:%s",
                native_window.isOnActiveSpace ? " yes" : " no");
    NSRect frame = native_window.frame;
    ImGui::Text("Frame: %.0f, %.0f  %.0f x %.0f", frame.origin.x,
                frame.origin.y, frame.size.width, frame.size.height);
}

void ApplicationDebugger::display_native_window_style_inspector() {
    ImGui::Begin("Native Window Inspector", &m_show_window_inspector);
    draw_native_window_style_inspector();
    ImGui::End();
}

#endif // NDEBUG