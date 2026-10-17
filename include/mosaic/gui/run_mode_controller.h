#pragma once

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

private:
    friend struct internal::ApplicationImpl;

    struct Deleter { void operator()(RunModeController* controller) const; };

    explicit RunModeController(internal::ApplicationImpl& app_impl);

    internal::ApplicationImpl& m_app_impl;
    std::size_t m_count = 0;
};

} // namespace mosaic