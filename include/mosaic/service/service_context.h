#pragma once

#include <memory>

#include <mosaic/service/command_system.h>
#include <mosaic/service/dispatcher.h>
#include <mosaic/service/event_system.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/icon_manager.h>
#include <mosaic/service/panel_manager.h>
#include <mosaic/service/style_system.h>
#include <mosaic/service/thread_pool.h>

namespace mosaic {

namespace internal { struct ApplicationImpl; }

class ServiceContext {
public:
    WorkDispatcher& dispatcher() { return m_dispatcher; }

    EventReceiver make_event_receiver();

    EventEmitter make_event_emitter();

    CommandSystem& command_system() { return m_command_system; }

    PanelManager& panel_manager() { return m_panel_manager; }

    FontManager& font_manager() { return m_font_manager; }

    IconManager& icon_manager() { return m_icon_manager; }

    StyleSystem& style_system() { return m_style_system; }

private:
    ServiceContext();

    friend struct internal::ApplicationImpl;

    ThreadPool m_thread_pool;
    WorkDispatcher m_dispatcher;
    std::shared_ptr<EventSystem> m_event_system;
    CommandSystem m_command_system;
    PanelManager m_panel_manager;
    FontManager m_font_manager;
    IconManager m_icon_manager;
    StyleSystem m_style_system;
};

} // namespace mosaic
