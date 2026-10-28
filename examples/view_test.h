#include <mosaic/gui/view.h>

struct UiTest {
    void display();
    void show() { m_open = true; }

private:
    bool m_open = false;
    std::shared_ptr<void> m_state;
    mosaic::Owner<mosaic::ui::View> m_root;
};