#include "thread_pool.hpp"

namespace gs {
namespace net {

void ThreadPool::Start(size_t num_threads) {
    if (running_.exchange(true)) return;
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerLoop, this);
    }
}

void ThreadPool::Stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void ThreadPool::Submit(std::function<void()> task) {
    if (!task || !running_.load()) return;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::WorkerLoop() {
    while (running_.load()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return !tasks_.empty() || !running_.load(); });
            if (!running_.load() && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        if (task) task();
    }
}

} // namespace net
} // namespace gs
