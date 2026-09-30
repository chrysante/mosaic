#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <magic_enum/magic_enum.hpp>

namespace mosaic {

template <typename E>
requires std::is_enum_v<E> inline constexpr std::size_t enum_count =
    magic_enum::enum_count<E>();

/// Wrapper around `std::array` that has as many elements are there are cases in
/// `E` and that can be indexed with values from `E`. This relies on the
/// `enum_count` template and `E` being well behaved
template <typename E, typename T>
struct EnumArray : std::array<T, enum_count<E>> {
    static_assert(sizeof(enum_count<E>));

    using std::array<T, enum_count<E>>::operator[];

    T& operator[](E e) {
        return (*this)[(std::size_t)e];
    }

    T const& operator[](E e) const {
        return (*this)[(std::size_t)e];
    }
};

template <typename E, typename T>
constexpr EnumArray<E, T> make_enum_array(T const& value) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return EnumArray<E, T>{((void)I, value)...};
    }(std::make_index_sequence<enum_count<E>>{});
}

} // namespace mosaic
