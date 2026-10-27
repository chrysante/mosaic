#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include <utl/vector.hpp>
#include <vml/vml.hpp>

namespace mosaic::ui {

// =============================================================================
// Pipe adapters
// =============================================================================

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

template <typename T> using Owner = std::unique_ptr<T>;

template <typename T, typename... Args> Owner<T> allocate(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// =============================================================================
// Layout primitives
// =============================================================================

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

    static constexpr SizeReq fixed(float value) {
        assert(std::isfinite(value));
        return {Type::fixed, value};
    }

    static constexpr SizeReq fill(float weight = 1.0f) {
        assert(std::isfinite(weight));
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

enum class Alignment { low, mid, high };

using Alignment2 = vml::vector<Alignment, 2>;

enum class Orientation { horizontal = 0, vertical = 1 };

// =============================================================================
// ValueOrRef<T>
// =============================================================================

template <typename T> class ValueOrRef {
public:
    static_assert(!std::is_reference_v<T>);

    ValueOrRef(T const& value)
        : m_value(value) {}
    ValueOrRef(T&& value)
        : m_value(std::move(value)) {}
    ValueOrRef(std::reference_wrapper<T> ref)
        : m_value(&ref.get()) {}

    T const& value() const {
        if (m_value.index() == 0) return std::get<0>(m_value);
        else return *std::get<1>(m_value);
    }

    T& value() { return const_cast<T&>(std::as_const(*this).get()); }

private:
    friend class ValueOrRef<T const>;

    std::variant<T, T*> m_value;
};

template <typename T> class ValueOrRef<T const> {
public:
    static_assert(!std::is_reference_v<T>);

    ValueOrRef(ValueOrRef<T> const& other)
        : m_value((T const*)nullptr) {
        if (m_value.index() == 0) m_value.emplace(std::get<0>(other.m_value));
        else m_value.emplace(std::get<1>(m_value));
    }

    ValueOrRef(ValueOrRef<T>&& other)
        : m_value((T const*)nullptr) {
        if (m_value.index() == 0)
            m_value.emplace(std::get<0>(std::move(other).m_value));
        else m_value.emplace(std::get<1>(m_value));
    }

    ValueOrRef(T const& value)
        : m_value(value) {}
    ValueOrRef(T&& value)
        : m_value(std::move(value)) {}
    ValueOrRef(std::reference_wrapper<T> ref)
        : m_value(&ref.get()) {}
    ValueOrRef(std::reference_wrapper<T const> ref)
        : m_value(&ref.get()) {}

    T const& value() const {
        if (m_value.index() == 0) return std::get<0>(m_value);
        else return *std::get<1>(m_value);
    }

private:
    std::variant<T, T const*> m_value;
};

// =============================================================================
// Property<T>
// =============================================================================

namespace internal {

template <typename T> struct PropertyInterface {
    virtual ~PropertyInterface() = default;
    virtual ValueOrRef<T const> get() const = 0;
    virtual void set(T&&) = 0;
    virtual std::unique_ptr<PropertyInterface> clone() const = 0;
};

template <typename T, std::convertible_to<T> U>
std::unique_ptr<PropertyInterface<T>> make_value_property(U&& value) {
    struct ValueProperty final : PropertyInterface<T> {
        explicit ValueProperty(T&& value)
            : value(std::move(value)) {}
        explicit ValueProperty(T const& value)
            : value(value) {}

        ValueOrRef<T const> get() const override { return std::cref(value); }

        void set(T&& new_value) override { value = std::move(new_value); }

        std::unique_ptr<PropertyInterface<T>> clone() const override {
            return std::make_unique<ValueProperty>(value);
        }

        T value;
    };
    return std::make_unique<ValueProperty>(std::forward<U>(value));
}

} // namespace internal

template <typename T> class Property {
    static_assert(std::is_object_v<T>, "T must be an object type");

    friend class Property<T const>;

public:
    Property(Property&&) noexcept = default;
    Property& operator=(Property&&) noexcept = default;

    Property(Property const& rhs)
        : m_impl(rhs.m_impl ? rhs.m_impl->clone() : nullptr) {}

    Property& operator=(Property const& rhs) {
        if (this != &rhs && rhs.m_impl) m_impl = rhs.m_impl->clone();
        return *this;
    }

    explicit Property(std::unique_ptr<internal::PropertyInterface<T>> impl)
        : m_impl(std::move(impl)) {}

    ValueOrRef<T const> get() const { return m_impl->get(); }

    T materialize() const { return get().value(); }

    void set(T new_value) { m_impl->set(std::move(new_value)); }

private:
    std::unique_ptr<internal::PropertyInterface<T>> m_impl;
};

template <typename T> class Property<T const> {
    static_assert(std::is_object_v<T>, "T must be an object type");

public:
    Property(Property&&) noexcept = default;
    Property& operator=(Property&&) noexcept = default;

    Property(Property const& rhs)
        : m_impl(rhs.m_impl ? rhs.m_impl->clone() : nullptr) {}

    Property& operator=(Property const& rhs) {
        if (this != &rhs && rhs.m_impl) m_impl = rhs.m_impl->clone();
        return *this;
    }

    Property(Property<T> const& rhs)
        : m_impl(rhs.m_impl ? rhs.m_impl->clone() : nullptr) {}

    Property(Property<T>&& rhs)
        : m_impl(std::move(rhs.m_impl)) {}

    // Const properties have an implicit value constructor
    template <std::convertible_to<T> U>
    Property(U&& value)
        : m_impl(internal::make_value_property<T>(std::forward<U>(value))) {}

    explicit Property(std::unique_ptr<internal::PropertyInterface<T>> impl)
        : m_impl(std::move(impl)) {}

    ValueOrRef<T const> get() const { return m_impl->get(); }

    T materialize() const { return get().value(); }

private:
    std::unique_ptr<internal::PropertyInterface<T>> m_impl;
};

template <typename T> Property<T> own(T value) {
    return Property<T>(internal::make_value_property<T>(std::move(value)));
}

template <typename T> Property<T> bind(T* pointer) {
    using U = std::remove_cv_t<T>;
    struct BindingProperty final : internal::PropertyInterface<U> {
        explicit BindingProperty(T* ptr)
            : ptr(ptr) {}

        ValueOrRef<U const> get() const override { return std::ref(*ptr); }

        void set(U&& new_value) override {
            if constexpr (!std::is_const_v<T>) *ptr = std::move(new_value);
            else assert(false);
        }

        std::unique_ptr<internal::PropertyInterface<U>> clone() const override {
            return std::make_unique<BindingProperty>(ptr);
        }

        T* ptr;
    };

    assert(pointer);
    return Property<T>(std::make_unique<BindingProperty>(pointer));
}

namespace internal {

template <typename Getter> struct ComputePropertyType {
    using GetterRet = std::invoke_result_t<Getter>;
    static_assert(!std::is_reference_v<GetterRet>);
    using Unwrapped = std::unwrap_ref_decay_t<GetterRet>;
    using type = std::remove_cvref_t<Unwrapped>;
};

} // namespace internal

template <typename Getter,
          typename T = internal::ComputePropertyType<Getter>::type>
Property<T const> computed(Getter&& get) {
    struct ComputedProperty final : internal::PropertyInterface<T> {
        explicit ComputedProperty(Getter&& getter)
            : getter(std::move(getter)) {}
        explicit ComputedProperty(Getter const& getter)
            : getter(getter) {}

        ValueOrRef<T const> get() const override { return std::invoke(getter); }

        void set(T&&) override { assert(false); }

        std::unique_ptr<internal::PropertyInterface<T>> clone() const override {
            return std::make_unique<ComputedProperty>(getter);
        }

        Getter getter;
    };
    return Property<T>(
        std::make_unique<ComputedProperty>(std::forward<Getter>(get)));
}

template <typename Getter, typename Setter,
          typename T = internal::ComputePropertyType<Getter>::type>
requires std::invocable<Setter, T> Property<T> computed(Getter&& get,
                                                        Setter&& set) {
    struct ComputedProperty final : internal::PropertyInterface<T> {
        explicit ComputedProperty(Getter&& getter, Setter&& setter)
            : getter(std::move(getter))
            , setter(std::move(setter)) {}
        explicit ComputedProperty(Getter const& getter, Setter const& setter)
            : getter(getter)
            , setter(setter) {}

        ValueOrRef<T const> get() const override { return std::invoke(getter); }

        void set(T&& new_value) override {
            std::invoke(setter, std::move(new_value));
        }

        std::unique_ptr<internal::PropertyInterface<T>> clone() const override {
            return std::make_unique<ComputedProperty>(getter, setter);
        }

        Getter getter;
        Setter setter;
    };

    return Property<T>(std::make_unique<ComputedProperty>(
        std::forward<Getter>(get), std::forward<Setter>(set)));
}

// =============================================================================
// Array
// =============================================================================

template <typename T, std::size_t inline_cap =
                          utl::default_inline_capacity<T, std::allocator<T>>>
class Array : public utl::small_vector<T, inline_cap> {
    using ImplType = utl::small_vector<T, inline_cap>;

public:
    Array() = default;
    template <std::input_iterator It, std::sentinel_for<It> Sen>
    Array(It begin, Sen end)
        : ImplType(begin, end) {}

    template <std::size_t N>
    Array(T (&&elements)[N])
        : ImplType(std::make_move_iterator(std::begin(elements)),
                   std::make_move_iterator(std::end(elements))) {}
};

// =============================================================================
// ViewContainer
// =============================================================================

template <typename V = View> class ViewContainer {
    auto elem_view() {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View& { return *v; });
    }
    auto elem_view() const {
        return std::views::transform(m_elements,
                                     [](auto& v) -> View const& { return *v; });
    }
    bool all_none_null() const {
        return std::ranges::all_of(m_elements,
                                   [](auto& v) { return v != nullptr; });
    }

public:
    ViewContainer() = default;
    ViewContainer(Array<Owner<V>> elements)
        : m_elements(std::move(elements)) {
        assert(all_none_null());
    }
    template <std::input_iterator It, std::sentinel_for<It> Sen>
    ViewContainer(It begin, Sen end)
        : m_elements(begin, end) {
        assert(all_none_null());
    }
    template <std::size_t N>
    ViewContainer(Owner<V> (&&elements)[N])
        : m_elements(std::move(elements)) {
        assert(all_none_null());
    }

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

    void push_back(Owner<V> view) {
        assert(view);
        m_elements.push_back(std::move(view));
    }

private:
    Array<Owner<V>> m_elements;
};

} // namespace mosaic::ui