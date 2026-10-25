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

    SizeReq2 size_requirement() const { return m_size_requirement; }

    void set_size_requirement(unsigned axis, SizeReq req) {
        m_size_requirement[axis] = req;
    }

protected:
    View(SizeReq2 size_requirement)
        : m_size_requirement(size_requirement) {}

private:
    SizeReq2 m_size_requirement;
};

} // namespace mosaic::ui