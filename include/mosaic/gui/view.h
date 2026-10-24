#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <initializer_list>
#include <memory>
#include <ranges>

#include <utl/vector.hpp>
#include <vml/vml.hpp>

namespace mosaic::ui {

template <typename T> using Ref = std::shared_ptr<T>;

template <typename T, typename... Args> static Ref<T> create(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

struct Rect {
    vml::float2 min() const { return origin; }
    vml::float2 max() const { return origin + size; }

    vml::float2 origin;
    vml::float2 size;
};

class SizeReq {
public:
    enum class Type { fit, fixed, fill };

    static constexpr SizeReq fit() { return {Type::fit, 0.0f}; }

    static constexpr SizeReq fixed(float value) { return {Type::fixed, value}; }

    static constexpr SizeReq fill() { return {Type::fill, 0.0f}; }

    constexpr Type type() const { return m_type; }

    /// \pre Must be of type `fixed`
    constexpr float value() const {
        assert(is_fixed());
        return m_value;
    }

    constexpr bool is_fit() const { return type() == Type::fit; }
    constexpr bool is_fixed() const { return type() == Type::fixed; }
    constexpr bool is_fill() const { return type() == Type::fill; }

private:
    constexpr SizeReq(Type type, float value)
        : m_type(type)
        , m_value(value) {}

    Type m_type;
    float m_value;
};

using SizeReq2 = vml::vector<SizeReq, 2>;

class View {
public:
    virtual ~View() = default;

    virtual void display(Rect frame) = 0;

    // The size the view naturally wants to display its content
    virtual vml::float2 intrinsic_size() const = 0;

    virtual SizeReq2 size_requirement() const { return m_layout_requirement; }

protected:
    View(SizeReq2 size_requirement)
        : m_layout_requirement(size_requirement) {}

private:
    SizeReq2 m_layout_requirement;
};

// =============================================================================
// Concrete Views
// =============================================================================

// ---  Spacer  ----------------------------------------------------------------

class Spacer : public View {
public:
    Spacer();
    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;
};

Ref<Spacer> spacer();

// ---  Button  ----------------------------------------------------------------

class Button : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit Button(std::string&& label, std::function<void()>&& on_click,
                    SizeReq2 size_requirement = default_layout_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::string m_label;
    std::function<void()> m_on_click;
};

Ref<Button>
button(std::string label, std::function<void()> action = {},
       SizeReq2 size_requirement = Button::default_layout_requirement);

// ---  Checkbox  --------------------------------------------------------------

class Checkbox : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit Checkbox(std::function<bool()>&& state,
                      std::function<void(bool)>&& on_change,
                      SizeReq2 size_requirement = default_layout_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

private:
    std::function<bool()> m_state;
    std::function<void(bool)> m_on_change;
};

Ref<Checkbox>
checkbox(std::function<bool()> state, std::function<void(bool)> on_change,
         SizeReq2 size_requirement = Checkbox::default_layout_requirement);

Ref<Checkbox>
checkbox(bool* value,
         SizeReq2 size_requirement = Checkbox::default_layout_requirement);

// ---  ViewContainer  ---------------------------------------------------------

class ViewContainer {
    auto elem_view() const {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View& { return *v; });
    }

public:
    ViewContainer() = default;
    ViewContainer(utl::small_vector<Ref<View>> elements)
        : m_elements(std::move(elements)) {}
    ViewContainer(std::initializer_list<Ref<View>> elements)
        : m_elements(elements) {}

    std::size_t size() const { return m_elements.size(); }

    bool empty() const { return m_elements.empty(); }

    View& operator[](std::size_t index) { return *m_elements[index]; }
    View const& operator[](std::size_t index) const {
        return *m_elements[index];
    }

    auto begin() const { return elem_view().begin(); }
    auto end() const { return elem_view().end(); }

    bool erase(View* view) {
        auto itr = std::ranges::find(m_elements, view,
                                     [](auto& v) { return v.get(); });
        if (itr == m_elements.end()) return false;
        m_elements.erase(itr);
        return true;
    }

    void push_back(Ref<View> view) { m_elements.push_back(std::move(view)); }

private:
    utl::small_vector<Ref<View>> m_elements;
};

// ---  Stack  -----------------------------------------------------------------

class StackView : public View {
public:
    enum class Orientation { horizontal = 0, vertical = 1 };

    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fit(),
                                                            SizeReq::fit()};

    explicit StackView(Orientation orientation, ViewContainer&& children,
                       SizeReq2 size_requirement);

    void display(Rect frame) override;

    vml::float2 intrinsic_size() const override;

    ViewContainer& children() { return m_children; }
    ViewContainer const& children() const { return m_children; }

private:
    Orientation m_orientation;
    ViewContainer m_children;
};

Ref<StackView>
vstack(ViewContainer children,
       SizeReq2 size_requirement = StackView::default_layout_requirement);

Ref<StackView>
hstack(ViewContainer children,
       SizeReq2 size_requirement = StackView::default_layout_requirement);

// ---  VScrollView  -----------------------------------------------------------

class VScrollView : public View {
public:
    static constexpr SizeReq2 default_layout_requirement = {SizeReq::fill(),
                                                            SizeReq::fill()};

    explicit VScrollView(ViewContainer&& children, SizeReq2 size_requirement);

    void display(Rect frame) override;
    vml::float2 intrinsic_size() const override;

    ViewContainer& children() { return m_children; }
    ViewContainer const& children() const { return m_children; }

private:
    ViewContainer m_children;
};

Ref<VScrollView>
vscroll(ViewContainer children,
        SizeReq2 size_requirement = VScrollView::default_layout_requirement);

} // namespace mosaic::ui