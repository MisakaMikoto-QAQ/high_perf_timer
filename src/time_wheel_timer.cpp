// src/time_wheel_timer.cpp
#include "time_wheel_timer.h"
#include <algorithm>

TimeWheelTimer::TimeWheelTimer(size_t slots, uint64_t tick_ms)
    : slots_(slots), tick_ms_(tick_ms), wheel_(slots) {
    last_advance_ = Clock::now();
}

size_t TimeWheelTimer::calc_slot(uint64_t delay_ms, uint64_t& rotations) const {
    // 向上取整，保证不早于 delay_ms 触发
    uint64_t ticks = (delay_ms + tick_ms_ - 1) / tick_ms_;
    if (ticks == 0) ticks = 1;
    rotations = ticks / slots_;
    return (current_ + ticks % slots_) % slots_;
}

void TimeWheelTimer::insert(uint64_t delay_ms, TimerId id,
                            uint64_t interval_ms, TimerCallback cb) {
    uint64_t rot = 0;
    size_t   s   = calc_slot(delay_ms, rot);
    wheel_[s].push_back(WheelTimer{id, rot, interval_ms, std::move(cb)});
    id_slot_[id] = s;
}

TimerId TimeWheelTimer::add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    TimerId id = next_id_++;
    insert(delay_ms, id, interval_ms, std::move(cb));
    return id;
}

bool TimeWheelTimer::cancel(TimerId id) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = id_slot_.find(id);
    if (it == id_slot_.end()) return false;
    size_t s = it->second;
    id_slot_.erase(it);

    auto& lst = wheel_[s];
    for (auto lit = lst.begin(); lit != lst.end(); ++lit) {
        if (lit->id == id) { lst.erase(lit); return true; }
    }
    return false;
}

size_t TimeWheelTimer::tick() {
    std::vector<WheelTimer> fired;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = (current_ + 1) % slots_;
        auto& slot = wheel_[current_];

        for (auto it = slot.begin(); it != slot.end(); ) {
            if (it->rotations > 0) {
                --it->rotations;
                ++it;
            } else {
                fired.push_back(std::move(*it));
                id_slot_.erase(fired.back().id);
                it = slot.erase(it);
            }
        }
    }

    for (auto& t : fired) {
        t.cb();
        if (t.interval_ms > 0) {
            add(t.interval_ms, t.cb, t.interval_ms);
        }
    }
    return fired.size();
}

size_t TimeWheelTimer::advance() {
    auto now = Clock::now();
    uint64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - last_advance_).count();
    uint64_t ticks = elapsed_ms / tick_ms_;
    size_t total = 0;
    for (uint64_t i = 0; i < ticks; ++i) total += tick();
    if (ticks > 0)
        last_advance_ += std::chrono::milliseconds(ticks * tick_ms_);
    return total;
}

size_t TimeWheelTimer::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return id_slot_.size();
}