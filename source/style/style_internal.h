#pragma once

#include "style/style.h"

namespace mosaic {

struct StyleDesc;

class StyleContext {
public:
    Style& style() { return m_style; }
    Style const& style() const { return m_style; }

    StyleColorId register_style_color(StyleVarMetadata metadata);
    StyleControlColorId register_style_control_color(StyleVarMetadata metadata);

    void push_color(StyleColorId id, Color value) {
        m_color_stack.push(id.index, value);
    }

    void pop_color(int count = 1) { m_color_stack.pop(count); }

    void push_control_color(StyleControlColorId id, ControlColor value) {
        m_control_color_stack.push(id.index, value);
    }

    void pop_control_color(int count = 1) { m_control_color_stack.pop(count); }

    bool stacks_empty() const {
        return m_color_stack.empty() && m_control_color_stack.empty();
    }

    static Style make_style(StyleDesc const& desc, StyleType style_type);

private:
    friend class StyleSystem;

    template <typename T> struct Stack {
        using Array = std::vector<T>;

        struct StackItem {
            unsigned index;
            T value;
        };

        explicit Stack(Array& array)
            : array(array) {}

        void push(unsigned index, T value) {
            stack.push({.index = index, .value = array[index]});
            array[index] = value;
        }

        void pop(int count) {
            assert(count >= 0 && (unsigned)count >= stack.size() &&
                   "Pop too many times");
            while (count-- > 0) {
                auto item = stack.pop();
                array[item.index] = item.value;
            }
        }

        bool empty() const { return stack.empty(); }

        Array& array;
        utl::stack<StackItem> stack;
    };

    Style m_style;
    Stack<Color> m_color_stack{m_style.m_colors};
    Stack<ControlColor> m_control_color_stack{m_style.m_control_colors};
};

StyleContext& get_global_style_context();

void apply_to_imgui(Style const& style);

StyleType get_current_system_style_type();

} // namespace mosaic