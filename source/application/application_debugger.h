#pragma once

#include "application/application.h"

#ifndef NDEBUG

namespace mosaic {

class Application::Debugger {
public:
    Debugger(Application* application)
        : application(application) {}

    void display();

    void toggle_open() { m_open = !m_open; }

private:
    void display_window_debugger();

    Application* application;
    bool m_open = false;
};

} // namespace mosaic

#endif // NDEBUG