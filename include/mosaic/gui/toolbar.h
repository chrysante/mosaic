#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <utl/common.hpp>
#include <vml/vml.hpp>

#include <mosaic/gui/font.h>

namespace mosaic {

class Command;

///
enum class ToolbarFlags { none = 0, no_decoration = 1 << 0 };

UTL_ENUM_OPERATORS(ToolbarFlags)

///
struct ToolbarStyle {
    float height = 26.0f;
    float button_alpha = 0.0f;
    float button_alpha_hovered = 0.5f;
    float button_alpha_active = 0.5f;

    /// Padding. Defaults to `ImGui::GetStyle().WindowPadding`
    std::optional<vml::float2> padding;

    ///
    ToolbarFlags flags = ToolbarFlags::none;

    /// Computed by `Toolbar`
    float actual_height;
};

/// Base class of all toolbar items
class ToolbarItem {
public:
    ToolbarItem() = default;
    ToolbarItem(ToolbarItem const&) = default;
    ToolbarItem(ToolbarItem&&) = default;
    virtual ~ToolbarItem() = default;

    /// \Returns a unique id.
    /// This is simply the index of the item in the toolbar
    int id() const {
        return m_id;
    }

    ///
    float get_width() const {
        return m_width;
    }

    /// \Returns `true` is this item is enabled. Disabled items are not
    /// displayed
    bool is_enabled() const {
        return do_is_enabled();
    }

    ///  \Returns an optional tooltip string for this item
    char const* get_tooltip() const {
        return do_get_tooltip();
    }

private:
    friend class Toolbar;

    virtual void display(ToolbarStyle const& style) = 0;
    virtual float calc_width(float height) const = 0;
    virtual bool do_is_enabled() const {
        return true;
    }
    virtual char const* do_get_tooltip() const {
        return nullptr;
    }

    int m_id;
    float m_width;
};

/// Mixin class that provides common functionality of toolbar items
template <typename Derived> class ToolbarItemMixin : public ToolbarItem {
public:
    /// Specifies a tooltip that will be displayed when this item is hovered
    Derived& tooltip(std::string text) {
        this->m_tooltip = [text] { return text.c_str(); };
        return as_derived();
    }

    /// \overload
    Derived& tooltip(std::function<char const*()> text) {
        this->m_tooltip = text;
        return as_derived();
    }

    /// Specifies whether this item is disabled
    Derived& enabled(bool value) {
        this->m_enabled = [value] { return value; };
        return as_derived();
    }

    /// \overload
    Derived& enabled(std::function<bool()> value) {
        this->m_enabled = value;
        return as_derived();
    }

protected:
    Derived& as_derived() {
        return *static_cast<Derived*>(this);
    }

private:
    bool do_is_enabled() const override {
        return !m_enabled || m_enabled();
    }
    char const* do_get_tooltip() const override {
        return m_tooltip ? m_tooltip() : nullptr;
    }

    std::function<char const*()> m_tooltip;
    std::function<bool()> m_enabled;
};

/// Common base class of toolbar buttons
template <typename Derived>
class ToolbarButtonMixin : public ToolbarItemMixin<Derived> {
public:
    /// Specify the action to perform when this button is clicked
    Derived& on_click(std::function<void()> action) {
        m_action = std::move(action);
        return this->as_derived();
    }

    Derived& button_flags(int flags) {
        m_button_flags |= flags;
        return this->as_derived();
    }

protected:
    void do_action() const;

    int m_button_flags = 0;

private:
    std::function<void()> m_action;
};

/// Toolbar text button
class ToolbarButton : public ToolbarButtonMixin<ToolbarButton> {
public:
    ToolbarButton(std::string label)
        : m_label(std::move(label)) {}

private:
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;

    std::string m_label;
};

///
class ToolbarIconButton : public ToolbarButtonMixin<ToolbarIconButton> {
public:
    /// Construct from an icon name
    ToolbarIconButton(std::string icon_name)
        : m_icon([=] { return icon_name.c_str(); }) {}

    /// Construct from a function that returns an icon name
    ToolbarIconButton(std::function<char const*()> icon_name_fn)
        : m_icon(icon_name_fn) {}

    /// Construct from an command and an icon name
    ToolbarIconButton(Command const* command, std::string icon_name);

private:
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;

    std::function<char const*()> m_icon;
};

/// A toolbar button that opens a dropdown menu
class ToolbarDropdownMenu : public ToolbarItemMixin<ToolbarDropdownMenu> {
public:
    ToolbarDropdownMenu();

    ToolbarDropdownMenu(char const* icon_name)
        : ToolbarDropdownMenu() {
        icon(icon_name);
    }

    /// Specifies the content callback
    ToolbarDropdownMenu& content(std::function<void()> content) {
        this->m_content = std::move(content);
        return *this;
    }

    /// The preview value should be the active option of the dropdown menu. If
    /// this is not set a down arrow will be used instead
    ToolbarDropdownMenu& previewValue(std::function<std::string()> value) {
        m_preview = value;
        return *this;
    }

    ///
    ToolbarDropdownMenu& icon(std::function<char const*()> icon_name_fn) {
        m_icon = icon_name_fn;
        return *this;
    }

    ///
    ToolbarDropdownMenu& icon(char const* icon_name) {
        return icon([=] { return icon_name; });
    }

    ///
    ToolbarDropdownMenu& minSize(vml::float2 size) {
        m_min_size = size;
        return *this;
    }

    ///
    ToolbarDropdownMenu& minWidth(float width) {
        m_min_size.x = width;
        return *this;
    }

protected:
    bool begin_combo(ToolbarStyle const& style);

    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;

    std::function<void()> m_content;
    std::function<std::string()> m_preview;
    std::function<char const*()> m_icon;
    std::string m_id;
    vml::float2 m_min_size = 0;
};

///
class ToolbarSeparator : public ToolbarItem {
private:
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;
};

/// Spacers are empty items that take up as much space as possible. They can be
/// used to split a toolbar into multiple sections
class ToolbarSpacer : public ToolbarItem {
private:
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;
};

/// Non-interactive text item
class ToolbarTextItem : public ToolbarItem {
public:
    ToolbarTextItem() = default;

    ToolbarTextItem(std::string text) {
        this->text(std::move(text));
    }

    /// Sets the displayed text
    ToolbarTextItem& text(std::string t) {
        return text([=] { return t.c_str(); });
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
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;

    char const* get_text() const;
    FontDesc get_font_desc() const;

    std::function<char const*()> m_text;
    std::function<FontDesc()> m_font_desc;
};

/// Empty item with fixed size
class ToolbarEmptyItem : public ToolbarItem {
public:
    ToolbarEmptyItem& width(float width) {
        m_width = width;
        return *this;
    }

private:
    void display(ToolbarStyle const& style) override;
    float calc_width(float height) const override;

    float m_width = 0;
};

/// Draws user defined content in a toolbar
class CustomToolbarElement : public ToolbarItem {
public:
    CustomToolbarElement(std::function<void(vml::float2 size)> content)
        : m_content(std::move(content)) {}

    CustomToolbarElement& width(float width) {
        m_custom_width = width;
        return *this;
    }

    /// Makes this element behave like a spacer
    CustomToolbarElement& useWidthAvail() {
        m_custom_width = 0;
        return *this;
    }

private:
    virtual void display(ToolbarStyle const&) {}
    virtual float calc_width(float) const {
        return m_custom_width;
    }

    std::function<void(vml::float2 size)> m_content;
    float m_custom_width = 0;
};

///
class Toolbar {
public:
    template <std::derived_from<ToolbarItem>... ItemTypes>
    Toolbar(ItemTypes... items) {
        (add_item(std::move(items)), ...);
    }

    /// \Returns the acccumulated width of all items including padding but
    /// ignoring all spacers. This is used by the dockspace for the triple
    /// toolbar layout
    float get_width_without_spacers() const;

    /// Displays the toolbar. Will fill available width unless parameter
    // \p width is provided
    void display(std::optional<float> width = std::nullopt) noexcept;

    /// \Returns the actual height of the toolbar including padding
    float get_height() const;

    ///
    void set_height(float value) {
        style.height = value;
    }

    ///
    ToolbarStyle const& get_style() const {
        return style;
    }

    ///
    void set_style(ToolbarStyle const& value) {
        style = value;
    }

    /// \Returns the resolved padding value
    vml::float2 get_padding() const;

    /// Overrides the padding
    void set_padding(std::optional<vml::float2> value) {
        style.padding = value;
    }

private:
    /// A block is a list of items without spacers
    struct Block {
        float width = 0;
        float offset = 0;
        size_t item_begin_index = 0;
        size_t item_end_index = 0;
        bool visible;
    };

    template <typename Item> void add_item(Item&& item) {
        all_items.push_back(std::make_unique<std::remove_cvref_t<Item>>(
            std::forward<Item>(item)));
    }

    void display_block(Block const&);

    /// Must be called after adding items
    void cook();

    /// Called every frame
    void calc_offsets(float totalWidth);

    std::vector<std::unique_ptr<ToolbarItem>> all_items;
    std::vector<ToolbarItem*> active_items;
    std::vector<Block> blocks;
    ToolbarStyle style{};
};

} // namespace mosaic
