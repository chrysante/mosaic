#include "gui/run_mode_controller.h"

#include <cassert>

#include "application/application_impl.h"
#include "application/frame_scheduler.h"

using namespace mosaic;

RunModeController::Lease RunModeController::enter_continuous_run_mode() {
    if (m_count++ == 0) {
        auto& scheduler = *m_app_impl.frame_scheduler;
        scheduler.run();
    }
    return Lease(this);
}

RunModeController::RunModeController(internal::ApplicationImpl& app_impl)
    : m_app_impl(app_impl) {}

void RunModeController::Deleter::operator()(
    RunModeController* controller) const {
    auto& app_impl = controller->m_app_impl;
    app_impl.service_context.dispatcher().on_main_thread([controller] {
        assert(controller->m_count > 0);
        if (--controller->m_count > 0) return;
        auto& scheduler = *controller->m_app_impl.frame_scheduler;
        scheduler.stop_running();
    });
}