#pragma once

#include <functional>

#include "application/application.h"

namespace mosaic {

class FrameScheduler {
public:
    static std::unique_ptr<FrameScheduler> create(RunLoopMode mode);

    using FrameCallback = std::function<void()>;

    virtual ~FrameScheduler() = default;

    virtual void start(FrameCallback callback) = 0;

    virtual void stop() = 0;

    virtual void set_mode(RunLoopMode mode) = 0;

    virtual RunLoopMode mode() const = 0;

    /// Request one frame, coalescing redundant requests
    virtual void request_frame() = 0;

    /// Request a number of frames
    virtual void request_frames(unsigned count) = 0;
};

} // namespace mosaic