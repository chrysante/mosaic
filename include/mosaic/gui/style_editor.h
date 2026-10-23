#include <memory>

#include <mosaic/gui/style.h>
#include <mosaic/support/export.h>

namespace mosaic {

class MOSAIC_EXPORT StyleEditor {
public:
    StyleEditor();
    ~StyleEditor();

    void draw();

private:
    struct Impl;

    void empty_editor();
    void new_style_modal();

    std::unique_ptr<Impl> m_impl;
};

} // namespace mosaic