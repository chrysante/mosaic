#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>

#include <utl/common.hpp>
#include <vml/vml.hpp>

#include <mosaic/gui/font.h>

namespace mosaic {

class Command;

///
enum class ToolbarFlags {
    none = 0,
    // If set, no separator line is drawn beneath the toolbar
    no_decoration = 1 << 0
};

UTL_ENUM_OPERATORS(ToolbarFlags)

// Toolbar
// ┌───────────────────────────────────────────────────────────────┐ ┬
// │                | padding.top                                  │ │
// │ ├──────────┤ ┌──────┐                  ┌──────┐ ├───────────┤ │ │
// │ padding.left │ Item │     <spacer>     │ Item │ padding.right │ height
// │              └──────┘                  └──────┘               │ │
// │                | padding.bottom                               │ │
// └───────────────────────────────────────────────────────────────┘ ┴

// Alpha multipliers for different button states
struct ToolbarButtonStyle {
    float alpha;
    float alpha_hovered;
    float alpha_active;
};

/// Style options to assign to a toolbar
struct ToolbarStyle {
    ToolbarButtonStyle button;

    /// -1 means default to `ImGui::GetStyle().WindowPadding`
    vml::float2 padding = {-1.0f, -1.0f};

    ///
    float item_spacing = 8.0f;

    ///
    ToolbarFlags flags = ToolbarFlags::none;
};

ToolbarStyle get_global_toolbar_style();

struct ToolbarItemDisplayData {
    ToolbarButtonStyle button_style;
    vml::float2 actual_size;
    int ID;
};

/// Interface for all toolbar items
class ToolbarItem {
public:
    virtual ~ToolbarItem() = default;

    void display(ToolbarItemDisplayData const& data) const { do_display(data); }

    /// \returns a unique id.
    /// This is simply the index of the item in the toolbar
    int ID() const { return m_id; }

    ///
    float width() const { return m_width; }

    ///
    void calc_width(float height) { m_width = do_calc_width(height); }

    /// \returns `true` is this item is enabled. Disabled items are not
    /// displayed
    bool enabled() const { return do_enabled(); }

    ///  \returns an optional tooltip string for this item
    char const* tooltip() const { return do_tooltip(); }

    bool is_spacer() const { return do_is_spacer(); }

protected:
    ToolbarItem(int ID)
        : m_id(ID) {}

private:
    virtual void do_display(ToolbarItemDisplayData const& data) const = 0;
    virtual float do_calc_width(float height) = 0;
    virtual bool do_enabled() const = 0;
    virtual char const* do_tooltip() const = 0;
    virtual bool do_is_spacer() const = 0;

    int m_id;
    float m_width = 0.0f;
};

template <typename ItemDisplay, typename State>
concept ToolbarItemDisplay =
    requires(State const state, ToolbarItemDisplayData const display_data,
             float height) {
    {ItemDisplay::display(state, display_data)}->std::same_as<void>;
    {ItemDisplay::calc_width(state, height)}->std::convertible_to<float>;
};

template <typename State>
concept ToolbarItemConcept = ToolbarItemDisplay<typename State::Display, State>;

namespace internal {

template <typename ItemState>
constexpr bool has_enabled = requires(ItemState const state) {
    {ItemState::Display::enabled(state)}->std::convertible_to<bool>;
};

template <typename ItemState>
constexpr bool has_tooltip = requires(ItemState const state) {
    {ItemState::Display::tooltip(state)}->std::convertible_to<char const*>;
};

template <typename ItemState>
constexpr bool has_is_spacer = requires(ItemState const state) {
    {ItemState::Display::is_spacer(state)}->std::convertible_to<bool>;
};

template <typename State> class ToolbarItemImpl : public ToolbarItem {
public:
    ToolbarItemImpl(State state, int ID)
        : ToolbarItem(ID)
        , m_state(std::move(state)) {}

private:
    using Display = State::Display;

    void do_display(ToolbarItemDisplayData const& data) const final {
        Display::display(m_state, data);
    }

    float do_calc_width(float height) final {
        return Display::calc_width(m_state, height);
    }

    bool do_enabled() const final {
        if constexpr (has_enabled<State>) return Display::enabled(m_state);
        else return true;
    }

    char const* do_tooltip() const final {
        if constexpr (has_tooltip<State>) return Display::tooltip(m_state);
        else return nullptr;
    }

    bool do_is_spacer() const final {
        if constexpr (has_is_spacer<State>) return Display::is_spacer(m_state);
        else return false;
    }

    State m_state;
};

template <typename ItemState>
std::unique_ptr<ToolbarItem> make_toolbar_item(ItemState&& state, int ID) {
    using StateClean = std::remove_cvref_t<ItemState>;
    return std::make_unique<ToolbarItemImpl<StateClean>>(std::move(state), ID);
}

} // namespace internal

/// Base class that provides common functionality for toolbar item
/// implementations
template <typename Derived> class ToolbarItemState {
public:
    struct Display;

    /// Specifies a tooltip that will be displayed when this item is hovered
    Derived& tooltip(std::string text) {
        m_tooltip = [text = std::move(text)] { return text.c_str(); };
        return as_derived();
    }

    /// \overload
    Derived& tooltip(std::function<char const*()> text) {
        m_tooltip = std::move(text);
        return as_derived();
    }

    /// Specifies whether this item is disabled
    Derived& enabled(bool value) {
        m_enabled = [value] { return value; };
        return as_derived();
    }

    /// \overload
    Derived& enabled(std::function<bool()> value) {
        m_enabled = std::move(value);
        return as_derived();
    }

protected:
    Derived& as_derived() { return *static_cast<Derived*>(this); }

private:
    std::function<char const*()> m_tooltip;
    std::function<bool()> m_enabled;
};

template <typename Derived> struct ToolbarItemState<Derived>::Display {
    static char const* tooltip(Derived const& state) {
        return state.m_tooltip ? state.m_tooltip() : nullptr;
    }

    static bool enabled(Derived const& state) {
        return state.m_enabled ? state.m_enabled() : true;
    }

    static bool is_spacer(Derived const&) { return false; }
};

/// Common base class of toolbar buttons
template <typename Derived>
class ToolbarButtonState : public ToolbarItemState<Derived> {
public:
    struct Display;

    /// Specify the action to perform when this button is clicked
    Derived& on_click(std::function<void()> action) {
        m_action = std::move(action);
        return this->as_derived();
    }

    Derived& button_flags(int flags) {
        m_button_flags |= flags;
        return this->as_derived();
    }

private:
    std::function<void()> m_action;
    int m_button_flags = 0;
};

template <typename Derived>
struct ToolbarButtonState<Derived>::Display
    : ToolbarItemState<Derived>::Display {
    static void do_action(Derived const& state) {
        if (state.m_action) state.m_action();
    }

    static int get_button_flags(Derived const& state) {
        return state.m_button_flags;
    }
};

/// Toolbar text button
class ToolbarButton : public ToolbarButtonState<ToolbarButton> {
public:
    struct Display;

    explicit ToolbarButton(std::string label)
        : m_label(std::move(label)) {}

private:
    friend struct ToolbarButtonDisplay;

    std::string m_label;
};

struct ToolbarButton::Display : ToolbarButtonState::Display {
    static void display(ToolbarButton const& button,
                        ToolbarItemDisplayData data);
    static float calc_width(ToolbarButton const& button, float height);
};

///
class ToolbarIconButton : public ToolbarButtonState<ToolbarIconButton> {
public:
    struct Display;

    /// Construct from an icon name
    explicit ToolbarIconButton(std::string icon_name)
        : m_icon_name([icon_name = std::move(icon_name)] {
            return icon_name.c_str();
        }) {}

    /// Construct from a function that returns an icon name
    explicit ToolbarIconButton(std::function<char const*()> icon_name_fn)
        : m_icon_name(std::move(icon_name_fn)) {}

    /// Construct from an command and an icon name
    explicit ToolbarIconButton(Command const* command, std::string icon_name);

private:
    friend struct ToolbarIconButtonDisplay;

    std::function<char const*()> m_icon_name;
};

struct ToolbarIconButton::Display : ToolbarButtonState::Display {
    static void display(ToolbarIconButton const& button,
                        ToolbarItemDisplayData data);
    static float calc_width(ToolbarIconButton const& button, float height);
};

/// A toolbar button that opens a dropdown menu
class ToolbarDropdownMenu : public ToolbarItemState<ToolbarDropdownMenu> {
public:
    struct Display;

    ToolbarDropdownMenu();

    explicit ToolbarDropdownMenu(char const* icon_name)
        : ToolbarDropdownMenu() {
        icon(icon_name);
    }

    /// Specifies the content callback
    ToolbarDropdownMenu& content(std::function<void()> content) {
        m_content = std::move(content);
        return *this;
    }

    /// The preview value should be the active option of the dropdown menu. If
    /// this is not set a down arrow will be used instead
    ToolbarDropdownMenu& preview_value(std::function<std::string()> value) {
        m_preview = std::move(value);
        return *this;
    }

    ///
    ToolbarDropdownMenu& icon(std::function<char const*()> icon_name_fn) {
        m_icon_name = std::move(icon_name_fn);
        return *this;
    }

    ///
    ToolbarDropdownMenu& icon(char const* icon_name) {
        return icon([=] { return icon_name; });
    }

    ///
    ToolbarDropdownMenu& min_size(vml::float2 size) {
        m_min_size = size;
        return *this;
    }

    ///
    ToolbarDropdownMenu& min_width(float width) {
        m_min_size.x = width;
        return *this;
    }

private:
    std::function<void()> m_content;
    std::function<std::string()> m_preview;
    std::function<char const*()> m_icon_name;
    std::string m_string_id;
    vml::float2 m_min_size = 0;
};

struct ToolbarDropdownMenu::Display : ToolbarItemState::Display {
    static bool begin_combo(ToolbarDropdownMenu const&,
                            ToolbarItemDisplayData const&);
    static void display(ToolbarDropdownMenu const&,
                        ToolbarItemDisplayData const&);
    static float calc_width(ToolbarDropdownMenu const&, float height);
};

///
struct ToolbarSeparator { struct Display; };

struct ToolbarSeparator::Display {
    static void display(ToolbarSeparator, ToolbarItemDisplayData const&) {}
    static float calc_width(ToolbarSeparator, float) { return 0.0f; }
};

/// Spacers are empty items that take up as much space as possible. They can be
/// used to split a toolbar into multiple sections
struct ToolbarSpacer { struct Display; };

struct ToolbarSpacer::Display {
    static void display(ToolbarSpacer, ToolbarItemDisplayData const&) {}
    static float calc_width(ToolbarSpacer, float) { return 0.0f; }
    static bool is_spacer(ToolbarSpacer) { return true; }
};

/// Non-interactive text item
class ToolbarTextItem {
public:
    struct Display;

    ToolbarTextItem() = default;

    ToolbarTextItem(std::string text) { this->text(std::move(text)); }

    /// Sets the displayed text
    ToolbarTextItem& text(std::string t) {
        return text([t = std::move(t)] { return t.c_str(); });
    }

    /// \overload
    ToolbarTextItem& text(std::function<char const*()> text_fn) {
        m_text = std::move(text_fn);
        return *this;
    }

    /// Sets the font used to render the text
    ToolbarTextItem& font(FontDesc font_desc) {
        return font([=] { return font_desc; });
    }

    /// \overload
    ToolbarTextItem& font(std::function<FontDesc()> font_desc_fn) {
        m_font_desc = std::move(font_desc_fn);
        return *this;
    }

private:
    char const* get_text() const;
    FontDesc get_font_desc() const;

    std::function<char const*()> m_text;
    std::function<FontDesc()> m_font_desc;
};

struct ToolbarTextItem::Display {
    static void display(ToolbarTextItem const&, ToolbarItemDisplayData const&);
    static float calc_width(ToolbarTextItem const&, float height);
};

/// Empty item with fixed size
class ToolbarEmptyItem {
public:
    struct Display;

    ToolbarEmptyItem& width(float width) {
        m_width = width;
        return *this;
    }

private:
    float m_width = 0;
};

struct ToolbarEmptyItem::Display {
    static void display(ToolbarEmptyItem, ToolbarItemDisplayData const&);
    static float calc_width(ToolbarEmptyItem, float height);
};

/// Draws user defined content in a toolbar
class CustomToolbarElement {
public:
    struct Display;

    CustomToolbarElement(std::function<void(vml::float2 size)> content)
        : m_content(std::move(content)) {}

    CustomToolbarElement& width(float width) {
        m_custom_width = width;
        return *this;
    }

    /// Makes this element behave like a spacer
    CustomToolbarElement& use_width_avail() {
        m_custom_width = 0.0f;
        return *this;
    }

private:
    std::function<void(vml::float2 size)> m_content;
    float m_custom_width = 0;
};

struct CustomToolbarElement::Display {
    static void display(CustomToolbarElement const&,
                        ToolbarItemDisplayData const&);
    static float calc_width(CustomToolbarElement const& item, float) {
        return item.m_custom_width;
    }
    static bool is_spacer(CustomToolbarElement const& item) {
        return item.m_custom_width == 0.0f;
    }
};

///
class Toolbar {
public:
    template <ToolbarItemConcept... ItemTypes> Toolbar(ItemTypes... items) {
        m_items.reserve(sizeof...(items));
        (add_item(std::move(items)), ...);
    }

    std::size_t num_items() const { return m_items.size(); }

    ToolbarItem const& item_at(std::size_t index) const {
        assert(index < num_items());
        return *m_items[index];
    }

    ToolbarItem& item_at(std::size_t index) {
        return const_cast<ToolbarItem&>(std::as_const(*this).item_at(index));
    }

private:
    template <typename ItemState> void add_item(ItemState&& item) {
        m_items.push_back(internal::make_toolbar_item(
            std::forward<ItemState>(item), static_cast<int>(m_items.size())));
    }

    std::vector<std::unique_ptr<ToolbarItem>> m_items;
};

} // namespace mosaic
