#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include <utl/vector.hpp>
#include <vml/vml.hpp>

#include <mosaic/support/memory.h>

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
    std::constructible_from<bool, P>;
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
    return std::invoke(adapter.action, std::forward<ViewPtr>(view));
}

template <PointerLike<View> ViewPtr, typename Action>
constexpr ViewPtr& operator|=(ViewPtr& view,
                              ViewPipeAdapter<Action> const& adapter) {
    return view = std::invoke(adapter.action, std::move(view));
}

template <typename Action1, typename Action2>
constexpr auto operator|(ViewPipeAdapter<Action1> adapter1,
                         ViewPipeAdapter<Action2> adapter2) {
    return ViewPipeAdapter{
        [adapter1 = std::move(adapter1),
         adapter2 =
             std::move(adapter2)]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            return std::invoke(
                adapter2.action,
                std::invoke(adapter1.action, std::forward<ViewPtr>(view)));
        }};
}

// =============================================================================
// Layout primitives
// =============================================================================

struct Rect {
    // The top-left corner of the rectangle (by convention)
    vml::float2 min() const { return origin; }

    // The bottom-right corner of the rectangle (by convention)
    vml::float2 max() const { return origin + size; }

    vml::float2 origin;
    vml::float2 size;
};

/// Determines the starting size used when allocating space to a fill child.
///
/// The basis affects the child's main-axis allocation. Intrinsic measurements
/// are still used for other purposes, such as cross-axis sizing.
enum class FillBasis : std::uint8_t {
    /// Start with a zero main-axis size. The child receives its weighted share
    /// of the space available for fill children.
    zero,

    /// Start with the child's intrinsic main-axis size. Any additional space
    /// is distributed among fill children according to their weights.
    /// The intrinsic size is a preferred starting size, not a minimum.
    intrinsic
};

class SizeSpec {
public:
    /// Specifies how a view's size is determined along one axis.
    enum class Mode : std::uint8_t {
        /// Use the view's intrinsic size.
        fit,

        /// Use an explicitly specified size.
        fixed,

        /// Participate in weighted allocation of available space.
        fill
    };

    /// Creates a sizing specification that uses the view's intrinsic size.
    static constexpr SizeSpec fit() { return {Mode::fit, 0.0f, {}}; }

    /// Creates a sizing specification with an explicit size.
    ///
    /// \param value The requested size, which must be finite and non-negative.
    static constexpr SizeSpec fixed(float value) {
        assert(std::isfinite(value));
        assert(value >= 0);
        return {Mode::fixed, value, {}};
    }

    /// Creates a sizing specification that participates in weighted allocation.
    ///
    /// Fill children share available space in proportion to their weights.
    /// The fill basis determines their starting main-axis sizes:
    /// - `FillBasis::zero`: start at zero and allocate space by weight.
    /// - `FillBasis::intrinsic`: start at intrinsic size and distribute
    ///   additional space by weight.
    ///
    /// If there is insufficient space, the layout's overflow policy determines
    /// how child allocations are reduced. The weight is relative to the weights
    /// of other fill children participating in the same allocation.
    ///
    /// \param weight The allocation weight, which must be finite and
    /// non-negative. A weight of zero does not claim any share of additional
    /// space.
    /// \param fill_basis Determines the starting main-axis allocation.
    static constexpr SizeSpec fill(float weight = 1.0f,
                                   FillBasis fill_basis = FillBasis::zero) {
        assert(std::isfinite(weight));
        assert(weight >= 0 && "fill weight must be non-negative");
        return {Mode::fill, weight, fill_basis};
    }

    /// Returns the sizing mode.
    constexpr Mode mode() const { return m_mode; }

    /// Returns the explicitly requested size.
    /// \pre Must be of type `fixed`.
    constexpr float fixed_size() const {
        assert(is_fixed());
        return m_value;
    }

    /// Returns the relative allocation weight.
    /// \pre Must be of type `fill`.
    constexpr float fill_weight() const {
        assert(is_fill());
        return m_value;
    }

    /// Returns the starting-size policy for a fill child.
    /// \pre Must be of type `fill`.
    constexpr FillBasis fill_basis() const {
        assert(is_fill());
        return m_fill_basis;
    }

    /// Returns whether the fill child starts with a zero main-axis size.
    /// \pre Must be of type `fill`.
    constexpr bool is_fill_zero() const {
        assert(is_fill());
        return fill_basis() == FillBasis::zero;
    }

    /// Returns whether the fill child starts with its intrinsic main-axis size.
    /// \pre Must be of type `fill`.
    constexpr bool is_fill_intrin() const {
        assert(is_fill());
        return fill_basis() == FillBasis::intrinsic;
    }

    /// Returns whether this specification uses intrinsic sizing.
    constexpr bool is_fit() const { return mode() == Mode::fit; }

    /// Returns whether this specification uses an explicit size.
    constexpr bool is_fixed() const { return mode() == Mode::fixed; }

    /// Returns whether this specification participates in weighted allocation.
    constexpr bool is_fill() const { return mode() == Mode::fill; }

private:
    constexpr SizeSpec(Mode type, float value, FillBasis fill_basis)
        : m_mode(type)
        , m_fill_basis(fill_basis)
        , m_value(value) {}

    Mode m_mode;
    FillBasis m_fill_basis;
    float m_value;
};

using SizeSpec2 = vml::vector<SizeSpec, 2>;

enum class Alignment { low, mid, high };

using Alignment2 = vml::vector<Alignment, 2>;

struct LayoutSpec {
    SizeSpec2 size_spec;
    Alignment2 align;
};

enum class Axis { horizontal = 0, vertical = 1 };

constexpr auto fit(Axis axis) {
    return ViewPipeAdapter{[axis]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
        if (view) view->set_size_spec(axis, SizeSpec::fit());
        return std::forward<ViewPtr>(view);
    }};
}

inline constexpr auto hfit = fit(Axis::horizontal);

inline constexpr auto vfit = fit(Axis::vertical);

constexpr auto fill_weight(Axis axis, float weight, FillBasis fill_basis) {
    return ViewPipeAdapter{
        [axis, weight, fill_basis]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view)
                view->set_size_spec(axis, SizeSpec::fill(weight, fill_basis));
            return std::forward<ViewPtr>(view);
        }};
}

constexpr auto fill(Axis axis, FillBasis fill_basis) {
    return fill_weight(axis, 1.0f, fill_basis);
}

inline constexpr auto hfill = fill(Axis::horizontal, FillBasis::zero);
inline constexpr auto hfill_intrin =
    fill(Axis::horizontal, FillBasis::intrinsic);

inline constexpr auto vfill = fill(Axis::vertical, FillBasis::zero);
inline constexpr auto vfill_intrin = fill(Axis::vertical, FillBasis::intrinsic);

constexpr inline auto hfill_weight(float weight, FillBasis fill_basis = {}) {
    return fill_weight(Axis::horizontal, weight, fill_basis);
}

constexpr inline auto vfill_weight(float weight, FillBasis fill_basis = {}) {
    return fill_weight(Axis::horizontal, weight, fill_basis);
}

constexpr inline auto fixed_size(Axis axis, float size) {
    return ViewPipeAdapter{
        [axis, size]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_size_spec(axis, SizeSpec::fixed(size));
            return std::forward<ViewPtr>(view);
        }};
}

constexpr inline auto fixed_width(float size) {
    return fixed_size(Axis::horizontal, size);
}

constexpr inline auto fixed_height(float size) {
    return fixed_size(Axis::vertical, size);
}

constexpr auto align(Axis axis, Alignment alignment) {
    return ViewPipeAdapter{
        [axis, alignment]<PointerLike<View> ViewPtr>(ViewPtr&& view) {
            if (view) view->set_align(axis, alignment);
            return std::forward<ViewPtr>(view);
        }};
}

constexpr auto align_left = align(Axis::horizontal, Alignment::low);

constexpr auto align_right = align(Axis::horizontal, Alignment::high);

constexpr auto align_top = align(Axis::vertical, Alignment::low);

constexpr auto align_bottom = align(Axis::vertical, Alignment::high);

constexpr auto hcenter = align(Axis::horizontal, Alignment::mid);

constexpr auto vcenter = align(Axis::vertical, Alignment::mid);

constexpr auto center = hcenter | vcenter;

// =============================================================================
// ValueOrRef<T>
// =============================================================================

/// A value that either owns a `T` or refers to an existing `T`.
///
/// `ValueOrRef<T>` stores either:
/// - An owned value, whose lifetime is managed by this object.
/// - A non-owning pointer to an externally owned value, supplied through
///   `std::reference_wrapper<T>`.
///
/// The referenced object is never copied or destroyed by `ValueOrRef`.
/// The caller must ensure that it outlives every use of the reference.
///
/// Copying an instance copies the owned value when it owns one, but preserves
/// the reference when it refers to external storage. Consequently, copying a
/// `ValueOrRef<T>` does not necessarily produce an independent value.
///
/// The non-const `value()` accessor permits modification of the stored value
/// or of the referenced object. The const accessor provides read-only access
/// through this instance.
///
/// @tparam T The non-const object type to store or reference. Must not be a
///           reference type.
template <typename T> class ValueOrRef {
public:
    /// Stores a copy of `value`.
    ///
    /// The stored value is owned by this instance and is independent of the
    /// original object.
    ValueOrRef(T const& value)
        : m_value(value) {}

    /// Takes ownership of `value` by moving it into this instance.
    ValueOrRef(T&& value)
        : m_value(std::move(value)) {}

    /// Stores a non-owning reference to the object wrapped by `ref`.
    ///
    /// The referenced object must outlive every use of this instance and
    /// any copies that preserve the reference.
    ValueOrRef(std::reference_wrapper<T> ref)
        : m_value(&ref.get()) {}

    /// Returns read-only access to the stored or referenced value.
    ///
    /// If this instance owns its value, the returned reference remains valid
    /// only while the owned value remains alive and is not invalidated.
    /// If this instance refers to external storage, the returned reference
    /// refers to that external object.
    T const& value() const {
        if (m_value.index() == 0) return std::get<0>(m_value);
        else return *std::get<1>(m_value);
    }

    /// Returns mutable access to the stored or referenced value.
    ///
    /// Modifications affect the owned value when this instance owns one, or
    /// the external object when this instance stores a reference.
    T& value() { return const_cast<T&>(std::as_const(*this).value()); }

private:
    friend class ValueOrRef<T const>;

    std::variant<T, T*> m_value;
};

/// A read-only view of a value that either owns a `T` or refers to an
/// existing `T const`.
///
/// This specialization has the same ownership model as `ValueOrRef<T>`,
/// but exposes no mutable accessor. Read-only access through this object does
/// not guarantee that a referenced object cannot be modified through another
/// alias.
///
/// Converting from `ValueOrRef<T>` preserves the active storage mode:
/// - An owned value is copied or moved into the const specialization.
/// - A reference remains a non-owning reference to the same object.
///
/// The caller remains responsible for the lifetime of referenced objects.
template <typename T> class ValueOrRef<T const> {
public:
    /// Copies or moves the active storage alternative from `other`.
    ///
    /// An owned value remains independently owned; a stored reference
    /// continues to refer to the same external object.
    ValueOrRef(ValueOrRef<T> const& other)
        : m_value((T const*)nullptr) {
        if (other.m_value.index() == 0)
            m_value.template emplace<0>(std::get<0>(other.m_value));
        else m_value.template emplace<1>(std::get<1>(other.m_value));
    }
    ValueOrRef(ValueOrRef<T>&& other)
        : m_value((T const*)nullptr) {
        if (other.m_value.index() == 0)
            m_value.template emplace<0>(std::get<0>(std::move(other).m_value));
        else m_value.template emplace<1>(std::get<1>(other.m_value));
    }

    /// Stores a copy of `value`.
    ValueOrRef(T const& value)
        : m_value(value) {}

    /// Takes ownership of `value` by moving it into this instance.
    ValueOrRef(T&& value)
        : m_value(std::move(value)) {}

    /// Stores a non-owning reference to an existing mutable object.
    ValueOrRef(std::reference_wrapper<T> ref)
        : m_value(&ref.get()) {}

    /// Stores a non-owning reference to an existing const object.
    ValueOrRef(std::reference_wrapper<T const> ref)
        : m_value(&ref.get()) {}

    /// Returns read-only access to the stored or referenced value.
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

/// A type-erased property that provides read access to a value and, for
/// mutable properties, an optional write operation.
///
/// A property delegates storage and access to a polymorphic implementation.
/// The provided factories create properties with different ownership and
/// update semantics:
///
/// - `own(value)` owns a value.
/// - `bind(pointer)` refers to externally owned storage.
/// - `computed(getter)` computes its value on demand and is read-only.
/// - `computed(getter, setter)` computes its value on demand and delegates
///   writes to the supplied setter.
///
/// Copying a property clones its implementation. This does not necessarily
/// copy the underlying data:
/// - An owned property gets an independent copy of its stored value.
/// - A bound property continues to refer to the same external object.
/// - A computed property gets copies of its getter and setter callables.
///   Any state captured by reference or shared through those callables
///   remains shared according to the captured objects' semantics.
///
/// A property does not extend the lifetime of external objects referenced by
/// a binding or by its callables. Those objects must remain valid for as long
/// as the property may access them.
///
/// Moving a property transfers its implementation. The moved-from property
/// should not be accessed unless it has subsequently been assigned a valid
/// implementation.
///
/// @tparam T The non-const object type represented by the property.
template <typename T> class Property {
    static_assert(std::is_object_v<T>, "T must be an object type");

    friend class Property<T const>;

public:
    /// Transfers the implementation from `rhs`.
    Property(Property&&) noexcept = default;

    /// Clones the implementation from `rhs`.
    Property(Property const& rhs)
        : m_impl(rhs.m_impl ? rhs.m_impl->clone() : nullptr) {}

    /// Replaces this property's implementation with a clone of `rhs`.
    Property& operator=(Property const& rhs) {
        if (this != &rhs) m_impl = rhs.m_impl ? rhs.m_impl->clone() : nullptr;
        return *this;
    }

    /// Transfers the implementation from `rhs`.
    Property& operator=(Property&&) noexcept = default;

    /// Constructs a property from an implementation.
    explicit Property(std::unique_ptr<internal::PropertyInterface<T>> impl)
        : m_impl(std::move(impl)) {}

    /// Returns read-only access to the current value.
    ///
    /// The returned `ValueOrRef` may own a value or refer to storage managed
    /// by the property's implementation. Its lifetime and aliasing behavior
    /// depend on that implementation.
    ValueOrRef<T const> get() const {
        assert(m_impl && "using moved-from object?");
        return m_impl->get();
    }

    /// Returns a value copy of the current property value.
    ///
    /// This materializes the result of `get()`; it does not expose a
    /// reference to the property's underlying storage.
    T materialize() const { return get().value(); }

    /// Replaces the property's value through its implementation.
    ///
    /// For an owned property, this replaces the owned value. For a bound
    /// property, this writes to the referenced object. For a computed
    /// property with a setter, this invokes that setter.
    ///
    /// Calling this on a read-only computed property violates its contract.
    void set(T new_value) {
        assert(m_impl && "using moved-from object?");
        m_impl->set(std::move(new_value));
    }

private:
    std::unique_ptr<internal::PropertyInterface<T>> m_impl;
};

/// A read-only property representing a value of type `T`.
///
/// This specialization provides the same read and materialization operations
/// as `Property<T>`, but does not expose `set()`.
///
/// It may own a value, refer to external storage, or compute its value.
/// Its copy and lifetime semantics depend on the underlying implementation,
/// just as for `Property<T>`.
template <typename T> class Property<T const> {
    static_assert(std::is_object_v<T>, "T must be an object type");

public:
    Property(Property&&) noexcept = default;
    Property& operator=(Property&&) noexcept = default;

    Property(Property const& rhs)
        : m_impl(rhs.m_impl ? rhs.m_impl->clone() : nullptr) {}

    Property& operator=(Property const& rhs) {
        if (this != &rhs) m_impl = rhs.m_impl ? rhs.m_impl->clone() : nullptr;
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

    ValueOrRef<T const> get() const {
        assert(m_impl && "using moved-from object?");
        return m_impl->get();
    }

    T materialize() const { return get().value(); }

private:
    std::unique_ptr<internal::PropertyInterface<T>> m_impl;
};

/// Creates a property that owns `value`.
///
/// Copying the returned property clones its stored value, so each copy
/// has independent owned storage.
template <typename T> Property<T> own(T value) {
    return Property<T>(internal::make_value_property<T>(std::move(value)));
}

/// Creates a property bound to externally owned storage.
///
/// The property does not own the pointed-to object. The caller must ensure
/// that the pointer remains valid for every access through the property.
///
/// For a pointer to mutable storage, writes update the referenced object.
/// For a pointer to const storage, the property is read-only in effect and
/// attempts to write are invalid.
///
/// @pre pointer must not be null.
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

/// Creates a read-only property whose value is obtained by invoking `get`.
///
/// The getter is stored inside the property and invoked when its value is
/// requested. If the getter returns a reference wrapper, the resulting
/// property will refer to the wrapped object; otherwise, its result is
/// copied.
///
/// Objects captured by reference must outlive every invocation of the getter.
template <typename Getter,
          typename T = internal::ComputePropertyType<Getter>::type>
Property<T const> computed(Getter&& get) {
    using GetterValue = std::remove_cvref_t<Getter>;
    struct ComputedProperty final : internal::PropertyInterface<T> {
        explicit ComputedProperty(GetterValue&& getter)
            : getter(std::move(getter)) {}
        explicit ComputedProperty(GetterValue const& getter)
            : getter(getter) {}

        ValueOrRef<T const> get() const override { return std::invoke(getter); }

        void set(T&&) override { assert(false); }

        std::unique_ptr<internal::PropertyInterface<T>> clone() const override {
            return std::make_unique<ComputedProperty>(getter);
        }

        GetterValue getter;
    };
    return Property<T>(
        std::make_unique<ComputedProperty>(std::forward<Getter>(get)));
}

/// Creates a property whose value is obtained by invoking `get` and whose
/// writes are delegated to `set`.
///
/// The setter is invoked when the property is assigned a new value. The
/// getter and setter are stored inside the property and copied when the
/// property implementation is cloned.
///
/// Any external state referenced or shared by the callables remains subject
/// to the lifetime and aliasing rules of those captures.
template <typename Getter, typename Setter,
          typename T = internal::ComputePropertyType<Getter>::type>
requires std::invocable<Setter, T> Property<T> computed(Getter&& get,
                                                        Setter&& set) {
    using GetterValue = std::remove_cvref_t<Getter>;
    using SetterValue = std::remove_cvref_t<Setter>;
    struct ComputedProperty final : internal::PropertyInterface<T> {
        explicit ComputedProperty(GetterValue getter, SetterValue setter)
            : getter(std::move(getter))
            , setter(std::move(setter)) {}

        ValueOrRef<T const> get() const override { return std::invoke(getter); }

        void set(T&& new_value) override {
            std::invoke(setter, std::move(new_value));
        }

        std::unique_ptr<internal::PropertyInterface<T>> clone() const override {
            return std::make_unique<ComputedProperty>(getter, setter);
        }

        GetterValue getter;
        SetterValue setter;
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
                                     [](auto& v) -> V& { return *v; });
    }
    auto elem_view() const {
        return std::views::transform(m_elements,
                                     [](auto& v) -> V const& { return *v; });
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

    void reserve(std::size_t cap) { m_elements.reserve(cap); }

    Array<Owner<V>> steal_elements() { return std::move(m_elements); }

private:
    Array<Owner<V>> m_elements;
};

} // namespace mosaic::ui