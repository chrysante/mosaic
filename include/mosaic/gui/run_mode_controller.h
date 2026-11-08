#pragma once

#include <chrono>
#include <cstddef>
#include <memory>

namespace mosaic {

namespace internal { struct ApplicationImpl; }

class RunModeController {
    struct Deleter;

public:
    using Lease = std::unique_ptr<RunModeController, Deleter>;

    /// Must be called from the main thread.
    /// The resulting lease can be destroyed asynchronously, however it must not
    /// outlive the application.
    [[nodiscard]] Lease enter_continuous_run_mode();

    template <typename R, typename P>
    void run_continuously_for(std::chrono::duration<R, P> duration) {
        run_continuously_for_impl(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration));
    }

    template <typename C, typename D>
    void run_continuously_until(std::chrono::time_point<C, D> time_point) {
        run_continuously_for(time_point - C::now());
    }

private:
    friend struct internal::ApplicationImpl;

    struct Deleter { void operator()(RunModeController* controller) const; };

    explicit RunModeController(internal::ApplicationImpl& app_impl);

    void run_continuously_for_impl(std::chrono::nanoseconds) {
        // unimplemented for now
    }

    internal::ApplicationImpl& m_app_impl;
    std::size_t m_count = 0;
};

RunModeController& get_run_mode_controller();

} // namespace mosaic