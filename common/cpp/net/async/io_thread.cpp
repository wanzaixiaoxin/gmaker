#include "io_thread.hpp"

namespace gs {
namespace net {
namespace async {

IOThread::IOThread() = default;

IOThread::~IOThread() {
    Stop();
}

bool IOThread::Start() {
    if (running_.exchange(true)) return false;

    loop_ = std::make_unique<AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    thread_ = std::thread(&IOThread::Run, this);
    return true;
}

void IOThread::Stop() {
    if (!running_.exchange(false)) return;
    if (loop_) {
        loop_->Stop();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void IOThread::Post(std::function<void()> task) {
    if (loop_) {
        loop_->Post(std::move(task));
    }
}

AsyncEventLoop* IOThread::Loop() const {
    return loop_.get();
}

bool IOThread::IsRunning() const {
    return running_.load();
}

void IOThread::Run() {
    if (loop_) {
        loop_->Run();
    }
}

} // namespace async
} // namespace net
} // namespace gs
