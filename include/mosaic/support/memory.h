#pragma once

#include <memory>

namespace mosaic {

template <typename T> using Owner = std::unique_ptr<T>;

template <typename T, typename... Args> Owner<T> allocate(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

} // namespace mosaic