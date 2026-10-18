// include/min_heap_timer.h
#pragma once

#include "timer.h"
#include <mutex>
#include <unordered_map>
#include <vector>

class MinHeapTimer {
public:
    explicit MinHeapTimer(size_t capacity = 64);

    // 相对时间添加；interval_ms>0 表示周期定时器
    TimerId add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);
    // 绝对时间添加
    TimerId add_at(TimePoint expire, TimerCallback cb, uint64_t interval_ms = 0);

    bool cancel(TimerId id);

    // 执行所有已经到期的定时器，返回执行个数
    size_t tick();

    // 距离下一个到期还有多少 ms；空堆返回 -1；已过期返回 0
    int64_t next_timeout_ms() const;

    size_t size() const;

private:
    void sift_up(size_t idx);
    void sift_down(size_t idx);
    void swap_node(size_t i, size_t j);
    void pop_top();

    std::vector<TimerPtr>                 heap_;   // 最小堆（按 expire 排序）
    std::unordered_map<TimerId, size_t>   index_;  // id -> 堆下标
    TimerId                               next_id_ = 1;
    mutable std::mutex                    mtx_;
};