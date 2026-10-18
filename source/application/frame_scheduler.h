#pragma once

#include <functional>

namespace mosaic {

class FrameScheduler {
public:
    static std::unique_ptr<FrameScheduler> create(std::function<void()> frame);

    virtual ~FrameScheduler() = default;

    /// Starts calling the frame callback in a continuous loop.
    /// Must only be called synchronously.
    virtual void run() = 0;

    /// Stops the continuous loop.
    /// Must only be called synchronously.
    /// Does not stop the application, frames may still be requested and `run()`
    /// can still be called to enter continuous run mode again.
    virtual void stop_running() = 0;

    /// Request a number of frames. When running continuously this request is
    /// ignored.
    /// Must only be called synchronously.
    virtual void request_frames(std::size_t count) = 0;

#ifndef NDEBUG
    virtual void draw_debugger() {}
#endif
};

} // namespace mosaic