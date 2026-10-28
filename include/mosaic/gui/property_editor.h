#pragma once

#include <string>

#include <mosaic/gui/view.h>
#include <mosaic/gui/view_support.h>
#include <mosaic/support/memory.h>

namespace mosaic::ui {

Owner<View> property(Property<std::string const> label, Owner<View> control);

Owner<View> section(ViewContainer<> children);

Owner<View> section(Property<std::string const> title,
                    ViewContainer<> children);

template <std::size_t N> Owner<View> section(Owner<View> (&&children)[N]) {
    return section(ViewContainer<>(std::move(children)));
}

template <std::size_t N>
Owner<View> section(Property<std::string const> title,
                    Owner<View> (&&children)[N]) {
    return section(std::move(title), ViewContainer<>(std::move(children)));
}

} // namespace mosaic::ui