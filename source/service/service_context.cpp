#include "service/service_context.h"

#include "service/event_emitter.h"
#include "service/event_receiver.h"
#include "service/event_system.h"

using namespace mosaic;

ServiceContext::ServiceContext()
    : m_dispatcher(m_thread_pool)
    , m_event_system(EventSystem::create()) {}

EventReceiver ServiceContext::make_event_receiver() {
    return m_event_system->make_receiver();
}

EventEmitter ServiceContext::make_event_emitter() {
    return m_event_system->make_emitter();
}