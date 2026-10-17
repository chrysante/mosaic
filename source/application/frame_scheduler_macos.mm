#include "application/frame_scheduler.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <memory>
#include <utility>

#import <AppKit/AppKit.h>

using namespace mosaic;

static bool is_main_thread() { return [NSThread isMainThread]; }

namespace {

struct FrameSchedulerMacOS final : FrameScheduler {
    FrameSchedulerMacOS(std::function<void()> frame)
        : frame_callback(frame) {}

    ~FrameSchedulerMacOS() {
        stop_running();
        [timer invalidate];
    }

    void run() override {
        assert(is_main_thread());
        if (pending_frames == -1) return;
        pending_frames = -1;
        start_timer();
    }

    void stop_running() override {
        assert(is_main_thread());
        if (pending_frames >= 0) return;
        pending_frames = 0;
    }

    void request_frames(std::size_t count) override {
        assert(is_main_thread());
        assert(count < INT_MAX);
        if (pending_frames == -1 || count == 0) return;
        pending_frames = std::max((int)count, pending_frames);
        start_timer();
    }

    void start_timer() {
        if (timer.isValid) return;
        void (^block)(NSTimer*) = ^(NSTimer* timer) {
          assert(is_main_thread());
          if (frame_callback) frame_callback();
          if (pending_frames > 0) --pending_frames;
          if (pending_frames == 0) [timer invalidate];
        };
        timer = [NSTimer timerWithTimeInterval:(1.0 / 60.0)
                                       repeats:YES
                                         block:block];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    }

    std::function<void()> frame_callback;
    NSTimer* timer = nil;
    // The number of frame we still need to process.
    // -1 means we are running continuously.
    int pending_frames = 0;
};

} // namespace

std::unique_ptr<FrameScheduler>
FrameScheduler::create(std::function<void()> frame) {
    return std::make_unique<FrameSchedulerMacOS>(std::move(frame));
}
