#pragma once

#include <vml/vml.hpp>

#include <mosaic/gui/view_support.h>

namespace mosaic::ui {

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

} // namespace mosaic::ui