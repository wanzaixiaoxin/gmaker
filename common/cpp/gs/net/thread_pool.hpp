#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace gs {
namespace net {

// ThreadPool 固定大小线程池
// 用于将 CPU 密集型任务（如 AES 加密）offload 出事件循环线程
class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { Stop(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void Start(size_t num_threads);
    void Stop();
    void Submit(std::function<void()> task);
    bool Running() const { return running_.load(); }

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
};

} // namespace net
} // namespace gs
