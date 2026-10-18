// include/timer_manager.h
#pragma once

#include "min_heap_timer.h"
#include <atomic>
#include <sys/epoll.h>

class TimerManager {
public:
    TimerManager() = default;
    ~TimerManager();

    bool init();

    TimerId add_timer(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms = 0);
    bool    cancel_timer(TimerId id);

    // 把普通 fd 也注册到同一个 epoll 实例
    bool add_fd(int fd, uint32_t events);
    bool del_fd(int fd);

    using FdCallback = std::function<void(int /*fd*/, uint32_t /*events*/)>;
    void set_fd_callback(FdCallback cb) { fd_cb_ = std::move(cb); }

    void run();
    void stop();

private:
    int                  epfd_      = -1;
    int                  wakeup_fd_ = -1;   // eventfd，用于唤醒 epoll
    std::atomic<bool>    running_{false};
    MinHeapTimer         timer_;
    FdCallback           fd_cb_;
};