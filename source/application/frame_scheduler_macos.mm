#include "application/frame_scheduler.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>

#import <AppKit/AppKit.h>

using namespace mosaic;

static bool is_main_thread() { return [NSThread isMainThread]; }

namespace {

struct MacOSFrameScheduler final : public FrameScheduler {
    explicit MacOSFrameScheduler(RunLoopMode mode)
        : mode_(mode) {}

    ~MacOSFrameScheduler() override { stop(); }

    void start(FrameCallback callback) override {
        assert(is_main_thread());
        stop();
        callback_ = std::move(callback);
        started_ = true;
        pending_frames_ = 0;
        if (mode_ == RunLoopMode::Poll) schedule_wait_frame();
    }

    void stop() override {
        assert(is_main_thread());
        // Invalidate previously scheduled callbacks.
        lifetime_.reset();
        lifetime_ = std::make_shared<int>(0);
        started_ = false;
        pending_frames_ = 0;
        wait_frame_scheduled_ = false;
        [frame_timer_ invalidate];
        frame_timer_ = nil;
        callback_ = {};
    }

    void set_mode(RunLoopMode mode) override {
        assert(is_main_thread());
        if (mode_ == mode) return;
        mode_ = mode;
        [frame_timer_ invalidate];
        frame_timer_ = nil;
        wait_frame_scheduled_ = false;
        if (!started_) return;
        if (mode_ == RunLoopMode::Poll || pending_frames_ > 0)
            schedule_wait_frame();
    }

    RunLoopMode mode() const override { return mode_; }

    void request_frame() override { request_frames(1); }

    void request_frames(unsigned count) override {
        assert(is_main_thread());
        if (!started_ || count == 0) return;
        pending_frames_ = std::max(pending_frames_, count);
        schedule_wait_frame();
    }

    void schedule_wait_frame() {
        assert(is_main_thread());
        if (!started_ || wait_frame_scheduled_) return;
        if (mode_ == RunLoopMode::Wait && pending_frames_ == 0) return;
        wait_frame_scheduled_ = true;
        // A weak lifetime token prevents a queued callback from
        // dereferencing the scheduler after stop()/destruction.
        std::weak_ptr<int> weak_lifetime = lifetime_;
        void (^block)(NSTimer* timer) = ^(NSTimer*) {
          if (weak_lifetime.expired()) return;
          run_wait_frame();
        };
        frame_timer_ = [NSTimer timerWithTimeInterval:(1.0 / 60.0)
                                              repeats:NO
                                                block:block];
        [[NSRunLoop mainRunLoop] addTimer:frame_timer_
                                  forMode:NSRunLoopCommonModes];
    }

    void run_wait_frame() {
        assert(is_main_thread());
        if (!started_) return;
        frame_timer_ = nil;
        wait_frame_scheduled_ = false;
        if (mode_ == RunLoopMode::Wait) {
            if (pending_frames_ == 0) return;
            --pending_frames_;
        }
        if (callback_) callback_();
        // Poll continuously; Wait only schedules frames that
        // have explicitly been requested.
        if (mode_ == RunLoopMode::Poll || pending_frames_ > 0)
            schedule_wait_frame();
    }

    void on_display_link() {
        assert(is_main_thread());
        if (started_ && callback_) callback_();
    }

    FrameCallback callback_;
    RunLoopMode mode_ = RunLoopMode::Wait;
    NSTimer* frame_timer_ = nil;
    unsigned pending_frames_ = 0;
    bool started_ = false;
    bool wait_frame_scheduled_ = false;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};

} // namespace

std::unique_ptr<FrameScheduler> FrameScheduler::create(RunLoopMode mode) {
    return std::make_unique<MacOSFrameScheduler>(mode);
}
