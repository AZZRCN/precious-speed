#pragma once
// thread_pool.h - 简单线程池 (C++17 MSVC)
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t n) : stop_(false) {
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lk(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }
    void enqueue(std::function<void()> task) {
        { std::lock_guard<std::mutex> lk(mtx_); tasks_.push(std::move(task)); }
        cv_.notify_one();
    }
private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
};
