#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <ranges>

#include <type_traits>
#include <utl/vector.hpp>
#include <vml/vml.hpp>

namespace mosaic::ui {

class View;

template <typename Action> struct ViewPipeAdapter { Action action; };

namespace internal {
template <typename P, typename V>
concept PointerClass =
    std::is_class_v<std::remove_cvref_t<P>> && requires(P const& p) {
    {p.operator->()}->std::convertible_to<V*>;
    {*p}->std::convertible_to<V&>;
};

template <typename P, typename V>
inline constexpr bool is_raw_pointer =
    std::is_pointer_v<std::remove_cvref_t<P>> &&
    std::convertible_to<std::remove_cvref_t<P>, V*>;

} // namespace internal

template <typename P, typename V>
concept PointerLike =
    internal::is_raw_pointer<P, V> || internal::PointerClass<P, V>;

template <typename V = View, PointerLike<V> ViewPtr, typename Action>
constexpr auto operator|(ViewPtr&& view,
                         ViewPipeAdapter<Action> const& adapter) {
    adapter.action(*view);
    return std::forward<ViewPtr>(view);
}

template <typename Action1, typename Action2>
constexpr auto operator|(ViewPipeAdapter<Action1> const& adapter1,
                         ViewPipeAdapter<Action2> const& adapter2) {
    return ViewPipeAdapter{[=]<typename V = View>(V& view) {
        adapter1.action(view);
        adapter2.action(view);
    }};
}

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

    static constexpr SizeReq fill(float weight = 1.0f) {
        assert(weight >= 0 && "fill weight must be non-negative");
        return {Type::fill, weight};
    }

    constexpr Type type() const { return m_type; }

    /// \pre Must be of type `fixed`
    constexpr float fixed_size() const {
        assert(is_fixed());
        return m_value;
    }

    /// \pre Must be of type `fill`
    constexpr float fill_weight() const {
        assert(is_fill());
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

enum class Alignment { left, top = left, center, right, bottom = right };

using Alignment2 = vml::vector<Alignment, 2>;

enum class Orientation { horizontal = 0, vertical = 1 };

template <typename T> class Binding {
public:
    Binding() = default;

    Binding(T* ptr)
        : m_state([ptr] { return ptr ? *ptr : T(); })
        , m_on_change([ptr](T const& new_value) {
            if (ptr) *ptr = new_value;
        }) {}

    Binding(std::function<T()> state, std::function<void(T const&)> on_change)
        : m_state(std::move(state))
        , m_on_change(std::move(on_change)) {}

    T get() const { return m_state ? m_state() : T(); }

    void on_change(T const& new_value) {
        if (m_on_change) m_on_change(new_value);
    }

private:
    std::function<T()> m_state;
    std::function<void(T const&)> m_on_change;
};

template <typename V = View> class ViewContainer {
    auto elem_view() const {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View& { return *v; });
    }

public:
    ViewContainer() = default;
    ViewContainer(utl::small_vector<Ref<V>> elements)
        : m_elements(std::move(elements)) {}
    ViewContainer(std::initializer_list<Ref<V>> elements)
        : m_elements(elements) {}

    std::size_t size() const { return m_elements.size(); }

    bool empty() const { return m_elements.empty(); }

    V& operator[](std::size_t index) { return *m_elements[index]; }
    V const& operator[](std::size_t index) const { return *m_elements[index]; }

    auto begin() const { return elem_view().begin(); }
    auto end() const { return elem_view().end(); }

    bool erase(V* view) {
        auto itr = std::ranges::find(m_elements, view,
                                     [](auto& v) { return v.get(); });
        if (itr == m_elements.end()) return false;
        m_elements.erase(itr);
        return true;
    }

    void push_back(Ref<V> view) { m_elements.push_back(std::move(view)); }

private:
    utl::small_vector<Ref<V>> m_elements;
};

} // namespace mosaic::ui