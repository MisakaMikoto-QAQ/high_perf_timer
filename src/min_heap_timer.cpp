// src/min_heap_timer.cpp
#include "min_heap_timer.h"

MinHeapTimer::MinHeapTimer(size_t capacity) {
    heap_.reserve(capacity);
    index_.reserve(capacity);
}

void MinHeapTimer::swap_node(size_t i, size_t j) {
    std::swap(heap_[i], heap_[j]);
    index_[heap_[i]->id()] = i;
    index_[heap_[j]->id()] = j;
}

void MinHeapTimer::sift_up(size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (heap_[idx]->expire() < heap_[parent]->expire()) {
            swap_node(idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

void MinHeapTimer::sift_down(size_t idx) {
    const size_t n = heap_.size();
    while (true) {
        size_t smallest = idx;
        size_t l = idx * 2 + 1;
        size_t r = idx * 2 + 2;
        if (l < n && heap_[l]->expire() < heap_[smallest]->expire()) smallest = l;
        if (r < n && heap_[r]->expire() < heap_[smallest]->expire()) smallest = r;
        if (smallest == idx) break;
        swap_node(idx, smallest);
        idx = smallest;
    }
}

TimerId MinHeapTimer::add_at(TimePoint expire, TimerCallback cb, uint64_t interval_ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    TimerId id = next_id_++;
    heap_.push_back(std::make_shared<Timer>(id, expire, std::move(cb), interval_ms));
    size_t idx = heap_.size() - 1;
    index_[id] = idx;
    sift_up(idx);
    return id;
}

TimerId MinHeapTimer::add(uint64_t delay_ms, TimerCallback cb, uint64_t interval_ms) {
    return add_at(Clock::now() + std::chrono::milliseconds(delay_ms),
                  std::move(cb), interval_ms);
}

bool MinHeapTimer::cancel(TimerId id) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = index_.find(id);
    if (it == index_.end()) return false;

    size_t idx  = it->second;
    size_t last = heap_.size() - 1;

    if (idx == last) {
        index_.erase(it);
        heap_.pop_back();
        return true;
    }

    // 与最后一个交换后 pop，再恢复堆序
    swap_node(idx, last);
    index_.erase(id);          // 被取消的 id 现在在 last 位置
    heap_.pop_back();

    if (idx > 0 && heap_[idx]->expire() < heap_[(idx - 1) / 2]->expire())
        sift_up(idx);
    else
        sift_down(idx);

    return true;
}

void MinHeapTimer::pop_top() {
    if (heap_.empty()) return;
    size_t last = heap_.size() - 1;
    if (last == 0) {
        index_.erase(heap_[0]->id());
        heap_.pop_back();
        return;
    }
    swap_node(0, last);
    index_.erase(heap_.back()->id());
    heap_.pop_back();
    sift_down(0);
}

size_t MinHeapTimer::tick() {
    std::vector<TimerPtr> fired;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto now = Clock::now();
        while (!heap_.empty() && heap_.front()->expire() <= now) {
            TimerPtr t = heap_.front();
            pop_top();
            fired.push_back(t);

            if (t->repeat()) {
                // 重新入堆；用 now 而不是旧 expire，避免“追赶”堆积
                t->set_expire(now + std::chrono::milliseconds(t->interval_ms()));
                heap_.push_back(t);
                size_t idx = heap_.size() - 1;
                index_[t->id()] = idx;
                sift_up(idx);
            }
        }
    }
    // 锁已经释放，回调里可以再次 add/cancel
    for (auto& t : fired) t->run();
    return fired.size();
}

int64_t MinHeapTimer::next_timeout_ms() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (heap_.empty()) return -1;
    auto now = Clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    heap_.front()->expire() - now).count();
    return diff < 0 ? 0 : diff;
}

size_t MinHeapTimer::size() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return heap_.size();
}