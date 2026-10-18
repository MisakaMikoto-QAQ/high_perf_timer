// include/timer.h
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

using Clock        = std::chrono::steady_clock;
using TimePoint    = Clock::time_point;
using TimerCallback = std::function<void()>;
using TimerId      = uint64_t;

// 单个定时器对象
class Timer {
public:
    Timer(TimerId id, TimePoint expire, TimerCallback cb, uint64_t interval_ms = 0)
        : id_(id), expire_(expire), callback_(std::move(cb)), interval_ms_(interval_ms) {}

    TimerId    id()          const { return id_; }
    TimePoint  expire()      const { return expire_; }
    uint64_t   interval_ms() const { return interval_ms_; }
    bool       repeat()      const { return interval_ms_ > 0; }

    void set_expire(TimePoint t) { expire_ = t; }
    void run() { if (callback_) callback_(); }

private:
    TimerId       id_;
    TimePoint     expire_;
    TimerCallback callback_;
    uint64_t      interval_ms_;   // 0 表示一次性
};

using TimerPtr = std::shared_ptr<Timer>;