#pragma once

#include <cassert>
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

enum class Orientation { horizontal = 0, vertical = 1 };

enum class Alignment { left, top = left, center, right, bottom = right };

template <typename V = class View> class ViewContainer {
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