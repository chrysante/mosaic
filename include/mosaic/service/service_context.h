#pragma once

#include <memory>

#include <mosaic/service/dispatcher.h>
#include <mosaic/service/event_system.h>
#include <mosaic/service/font_manager.h>
#include <mosaic/service/thread_pool.h>

namespace mosaic {

class ServiceContext {
public:
    ServiceContext();

    WorkDispatcher& dispatcher() { return m_dispatcher; }

    EventReceiver make_event_receiver();

    EventEmitter make_event_emitter();

    FontManager& font_manager() { return m_font_manager; }

private:
    friend class Application;

    ThreadPool m_thread_pool;
    WorkDispatcher m_dispatcher;
    std::shared_ptr<EventSystem> m_event_system;
    FontManager m_font_manager;
};

} // namespace mosaic
