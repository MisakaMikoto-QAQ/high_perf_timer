// src/timer_manager.cpp
#include "timer_manager.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <unistd.h>
#include <sys/eventfd.h>
#include <vector>

TimerManager::~TimerManager() {
    stop();
    if (epfd_      >= 0) ::close(epfd_);
    if (wakeup_fd_ >= 0) ::close(wakeup_fd_);
}

bool TimerManager::init() {
    epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd_ < 0) return false;

    wakeup_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeup_fd_ < 0) {
        ::close(epfd_); epfd_ = -1;
        return false;
    }

    epoll_event ev{};
    ev.events   = EPOLLIN;
    ev.data.fd  = wakeup_fd_;
    if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) < 0) {
        ::close(wakeup_fd_); wakeup_fd_ = -1;
        ::close(epfd_);      epfd_      = -1;
        return false;
    }
    return true;
}

TimerId TimerManager::add_timer(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms) {
    TimerId id = timer_.add(delay_ms, std::move(cb), interval_ms);
    uint64_t one = 1;
    if (wakeup_fd_ >= 0) {
        ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
        (void)n;
    }
    return id;
}

bool TimerManager::cancel_timer(TimerId id) {
    return timer_.cancel(id);
}

bool TimerManager::add_fd(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events  = events;
    ev.data.fd = fd;
    return ::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool TimerManager::del_fd(int fd) {
    return ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

void TimerManager::run() {
    running_ = true;
    constexpr int MAX_EVENTS = 1024;
    std::vector<epoll_event> events(MAX_EVENTS);

    while (running_) {
        int64_t timeout = timer_.next_timeout_ms();
        int wait_ms = -1;
        if (timeout >= 0) {
            wait_ms = (timeout > INT_MAX) ? INT_MAX : static_cast<int>(timeout);
        }

        int n = ::epoll_wait(epfd_, events.data(), MAX_EVENTS, wait_ms);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            int      fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == wakeup_fd_) {
                uint64_t v = 0;
                ssize_t r = ::read(wakeup_fd_, &v, sizeof(v));
                (void)r;
                continue;
            }
            if (fd_cb_) fd_cb_(fd, ev);
        }

        // 处理到期的定时器（回调里可以继续 add/cancel）
        timer_.tick();
    }
}

void TimerManager::stop() {
    running_ = false;
    uint64_t one = 1;
    if (wakeup_fd_ >= 0) {
        ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
        (void)n;
    }
}