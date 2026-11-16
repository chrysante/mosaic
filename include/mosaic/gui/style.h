#pragma once

#include <cstdint>

#include <utl/stack.hpp>
#include <vml/vml.hpp>

#include <mosaic/support/enum.h>

namespace mosaic {

enum class StyleColor {
    text,
    text_disabled,
    text_link,

    border,
    separator,
    scrollbar,

    docking_preview,

    window_background,
};

enum class StyleColorControl { Default, accent, toolbar, tab, tab_inactive };

struct StyleDesc;

// Concrete style colors and sizes

struct Color { std::uint32_t value; };

struct ControlColor {
    Color background;
    Color background_hovered;
    Color background_active;
    Color border;
    Color text;
};

enum struct StyleType { light, dark };

struct Style_ {
    StyleType style_type;

    EnumArray<StyleColor, Color> color;
    EnumArray<StyleColorControl, ControlColor> control_color;
};

Style_ const& get_global_style2();

void push_style_color(StyleColor index, Color value);
void pop_style_color();
void push_style_control_color(StyleColorControl index, ControlColor value);
void pop_style_control_color();

// TODO: Move this helper class somewhere more private
struct StyleContext {
    Style_ const& style() const { return m_style; }

    void push_color(StyleColor index, Color value) {
        m_color_stack.push(index, value);
    }

    void pop_color(int count = 1) { m_color_stack.pop(count); }

    void push_control_color(StyleColorControl index, ControlColor value) {
        m_control_color_stack.push(index, value);
    }

    void pop_control_color(int count = 1) { m_control_color_stack.pop(count); }

    bool stacks_empty() const {
        return m_color_stack.empty() && m_control_color_stack.empty();
    }

private:
    friend class StyleSystem;

    template <typename Index, typename T> struct Stack {
        struct StackItem {
            Index index;
            T value;
        };

        explicit Stack(T* array)
            : array(array) {}

        void push(Index index, T value) {
            stack.push({index, array[(std::size_t)index]});
            array[(std::size_t)index] = value;
        }

        void pop(int count) {
            assert(count >= 0 && (unsigned)count >= stack.size() &&
                   "Pop too many times");
            while (count-- > 0) {
                auto item = stack.pop();
                array[(std::size_t)item.index] = item.value;
            }
        }

        bool empty() const { return stack.empty(); }

        T* array;
        utl::stack<StackItem> stack;
    };

    Style_ m_style;
    Stack<StyleColor, Color> m_color_stack{m_style.color.data()};
    Stack<StyleColorControl, ControlColor> m_control_color_stack{
        m_style.control_color.data()};
};

// TODO: Move these helper functions somewhere more private
Style_ compute_from_desc(StyleDesc const& style_desc, StyleType style_type);
void apply_to_imgui(Style_ const& style);
StyleType get_current_system_style_type();

} // namespace mosaic