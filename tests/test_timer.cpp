// tests/test_timer.cpp
#include "min_heap_timer.h"
#include "time_wheel_timer.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

static void test_min_heap_basic() {
    std::cout << "[TEST] min_heap basic ... ";
    MinHeapTimer t;
    std::atomic<int> c{0};
    t.add(50,  [&]{ c++; });
    t.add(100, [&]{ c++; });

    std::this_thread::sleep_for(150ms);
    t.tick();
    assert(c == 2);
    std::cout << "OK\n";
}

static void test_min_heap_cancel() {
    std::cout << "[TEST] min_heap cancel ... ";
    MinHeapTimer t;
    std::atomic<int> c{0};
    TimerId id = t.add(50, [&]{ c++; });
    assert(t.cancel(id));
    std::this_thread::sleep_for(120ms);
    t.tick();
    assert(c == 0);
    assert(t.size() == 0);
    std::cout << "OK\n";
}

static void test_min_heap_repeat() {
    std::cout << "[TEST] min_heap repeat ... ";
    MinHeapTimer t;
    std::atomic<int> c{0};
    TimerId id = t.add(50, [&]{ c++; }, 50);

    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(60ms);
        t.tick();
    }
    t.cancel(id);
    assert(c >= 3);
    std::cout << "OK (count=" << c << ")\n";
}

static void test_time_wheel() {
    std::cout << "[TEST] time wheel ... ";
    // 10 槽 x 50ms = 覆盖 500ms
    TimeWheelTimer tw(10, 50);
    std::atomic<int> c{0};
    tw.add(100, [&]{ c++; });
    tw.add(250, [&]{ c++; });

    std::this_thread::sleep_for(300ms);
    tw.advance();
    assert(c == 2);
    std::cout << "OK\n";
}

static void test_time_wheel_wrap() {
    std::cout << "[TEST] time wheel wrap ... ";
    // 5 槽 x 20ms = 100ms 覆盖；加一个 250ms 的，需要转 2 圈多
    TimeWheelTimer tw(5, 20);
    std::atomic<int> c{0};
    tw.add(250, [&]{ c++; });

    std::this_thread::sleep_for(300ms);
    tw.advance();
    assert(c == 1);
    std::cout << "OK\n";
}

int main() {
    test_min_heap_basic();
    test_min_heap_cancel();
    test_min_heap_repeat();
    test_time_wheel();
    test_time_wheel_wrap();
    std::cout << "\nAll tests passed.\n";
    return 0;
}