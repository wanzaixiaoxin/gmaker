#pragma once

#include "event_loop.hpp"
#include <thread>
#include <memory>
#include <functional>
#include <atomic>

namespace gs {
namespace net {
namespace async {

// IOThread 封装一个独立线程 + AsyncEventLoop
// 为未来的多 Reactor 架构预留抽象（当前单线程模型下直接使用即可）
class IOThread {
public:
    IOThread();
    ~IOThread();

    IOThread(const IOThread&) = delete;
    IOThread& operator=(const IOThread&) = delete;

    bool Start();
    void Stop();
    void Post(std::function<void()> task);
    AsyncEventLoop* Loop() const;
    bool IsRunning() const;

private:
    void Run();

    std::unique_ptr<AsyncEventLoop> loop_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace async
} // namespace net
} // namespace gs
