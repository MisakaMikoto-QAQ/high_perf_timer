// examples/epoll_server.cpp
#include "timer_manager.h"

#include <csignal>
#include <iostream>

static TimerManager* g_mgr = nullptr;

static void on_signal(int) {
    if (g_mgr) g_mgr->stop();
}

int main() {
    TimerManager mgr;
    if (!mgr.init()) {
        std::cerr << "TimerManager::init failed\n";
        return 1;
    }
    g_mgr = &mgr;
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // 周期任务：每 500ms 打印一次
    mgr.add_timer(500, []{
        std::cout << "[timer] 500ms tick\n";
    }, 500);

    // 一次性任务：3 秒后停止
    mgr.add_timer(3000, [&mgr]{
        std::cout << "[timer] 3s reached, stop\n";
        mgr.stop();
    });

    std::cout << "TimerManager running... (Ctrl+C to exit)\n";
    mgr.run();
    std::cout << "TimerManager exited\n";
    return 0;
}