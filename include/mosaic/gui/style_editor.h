#include <mosaic/gui/style_desc.h>
#include <mosaic/gui/view.h>
#include <mosaic/support/export.h>

namespace mosaic {

class StyleSystem;

namespace internal { struct StyleEditorImpl; }

class MOSAIC_EXPORT StyleEditor {
public:
    explicit StyleEditor(StyleSystem& style_system);
    ~StyleEditor();

    void draw();

private:
    std::unique_ptr<internal::StyleEditorImpl> m_impl;
};

} // namespace mosaic