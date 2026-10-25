#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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

template <PointerLike<View> ViewPtr, typename Action>
constexpr auto operator|(ViewPtr&& view,
                         ViewPipeAdapter<Action> const& adapter) {
    adapter.action(*view);
    return std::forward<ViewPtr>(view);
}

template <PointerLike<View> ViewPtr, typename Action>
constexpr ViewPtr const& operator|=(ViewPtr const& view,
                                    ViewPipeAdapter<Action> const& adapter) {
    adapter.action(*view);
    return view;
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

template <typename T> class ValueOrRef {
public:
    static_assert(!std::is_reference_v<T>);

    ValueOrRef(T const& value)
        : m_value(value) {}
    ValueOrRef(T&& value)
        : m_value(std::move(value)) {}
    ValueOrRef(T const* pointer)
        : m_value(pointer) {
        assert(pointer);
    }
    ValueOrRef(std::reference_wrapper<T const> ref)
        : m_value(&ref.get()) {}

    operator T const&() const { return get(); }

    T const& get() const {
        if (m_value.index() == 0) return std::get<0>(m_value);
        else return *std::get<1>(m_value);
    }

private:
    std::variant<T, T const*> m_value;
};

template <typename T> class Binding {
public:
    using Getter = std::function<ValueOrRef<T>()>;
    using Setter = std::function<void(T)>;

    explicit Binding(Getter&& get, Setter&& set)
        : m_get(std::move(get))
        , m_set(std::move(set)) {
        assert(m_get);
    }

    ValueOrRef<T> get() const {
        assert(m_get);
        return m_get();
    }

    void set(T new_value) { m_set(std::move(new_value)); }

private:
    template <typename> friend class Binding;

    Getter m_get;
    Setter m_set;
};

template <typename T> class Binding<T const> {
public:
    using Getter = std::function<ValueOrRef<T>()>;

    explicit Binding(Getter&& get)
        : m_get(std::move(get)) {
        assert(m_get);
    }

    Binding(Binding<T> const& rhs)
        : m_get(rhs.m_get) {}

    Binding(Binding<T>&& rhs)
        : m_get(std::move(rhs).m_get) {}

    ValueOrRef<T> get() const {
        assert(m_get);
        return m_get();
    }

private:
    Getter m_get;
};

template <typename T> Binding<T> bind(T* pointer) {
    assert(pointer);
    return Binding<T>(
        [pointer] { return std::cref(*pointer); },
        [pointer](T new_value) { *pointer = std::move(new_value); });
}

template <typename T> Binding<T const> bind(T const* pointer) {
    assert(pointer);
    return Binding<T const>([pointer] { return std::cref(*pointer); });
}

template <typename T> Binding<T> own(T value) {
    auto storage = std::make_shared<T>(std::move(value));
    return Binding<T>(
        [storage] { return std::cref(*storage); },
        [storage](T new_value) { *storage = std::move(new_value); });
}

namespace internal {
template <typename Getter>
using GetterType = std::remove_cvref_t<std::invoke_result_t<Getter>>;
}

template <typename Getter, typename Setter,
          typename T = internal::GetterType<Getter>>
requires std::invocable<Setter, T> Binding<T> computed(Getter&& get,
                                                       Setter&& set) {
    return Binding<T>(std::forward<Getter>(get), std::forward<Setter>(set));
}

template <typename Getter, typename T = internal::GetterType<Getter>>
Binding<T const> computed(Getter&& get) {
    return Binding<T const>(std::forward<Getter>(get));
}

template <typename V = View> class ViewContainer {
    auto elem_view() {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View& { return *v; });
    }
    auto elem_view() const {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View const& { return *v; });
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

    auto begin() { return elem_view().begin(); }
    auto begin() const { return elem_view().begin(); }
    auto end() { return elem_view().end(); }
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