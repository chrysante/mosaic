#include <mosaic/gui/containers.h>
#include <mosaic/gui/view.h>
#include <mosaic/gui/widgets.h>

struct UiTest {
    void display();
    void show() { m_open = true; }

private:
    bool m_open = false;
    std::shared_ptr<void> m_state;
    mosaic::ui::Ref<mosaic::ui::View> m_root;
};