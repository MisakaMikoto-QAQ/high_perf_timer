// include/time_wheel_timer.h
#pragma once

#include "timer.h"
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

class TimeWheelTimer {
public:
    // slots 个槽，每个槽 tick_ms 毫秒
    explicit TimeWheelTimer(size_t slots = 60, uint64_t tick_ms = 1000);

    TimerId add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);
    bool    cancel(TimerId id);

    // 指针前进一格，返回本格触发的定时器数量
    size_t  tick();

    // 按真实流逝时间推进若干格
    size_t  advance();

    size_t  size() const;

private:
    struct WheelTimer {
        TimerId       id;
        uint64_t      rotations;    // 还需要转多少圈
        uint64_t      interval_ms;  // 0 表示一次性
        TimerCallback cb;
    };

    size_t calc_slot(uint64_t delay_ms, uint64_t& rotations) const;
    void   insert(uint64_t delay_ms, TimerId id, uint64_t interval_ms, TimerCallback cb);

    size_t                                slots_;
    uint64_t                              tick_ms_;
    size_t                                current_ = 0;
    std::vector<std::list<WheelTimer>>    wheel_;
    std::unordered_map<TimerId, size_t>   id_slot_;
    TimerId                               next_id_ = 1;
    TimePoint                             last_advance_;
    mutable std::mutex                    mtx_;
};