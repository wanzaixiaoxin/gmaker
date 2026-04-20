#include "event_loop.hpp"

#include <uv.h>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

namespace gs {
namespace net {
namespace async {

struct AsyncEventLoop::Impl {
    std::mutex task_mtx;
    std::deque<Task> tasks;
    std::thread::id loop_tid;
    std::atomic<bool> stopping{false};
};

AsyncEventLoop::AsyncEventLoop() : impl_(std::make_unique<Impl>()) {}

AsyncEventLoop::~AsyncEventLoop() {
    if (loop_) {
        // 关闭所有剩余 handle
        uv_walk(loop_, OnWalkClose, nullptr);
        uv_run(loop_, UV_RUN_DEFAULT);
        uv_loop_close(loop_);
        delete loop_;
        loop_ = nullptr;
    }
}

bool AsyncEventLoop::Init() {
    if (loop_) return true;
    loop_ = new uv_loop_t;
    if (uv_loop_init(loop_) != 0) {
        delete loop_;
        loop_ = nullptr;
        return false;
    }

    wake_handle_ = new uv_async_t;
    if (uv_async_init(loop_, wake_handle_, OnAsyncWake) != 0) {
        uv_loop_close(loop_);
        delete loop_;
        loop_ = nullptr;
        delete wake_handle_;
        wake_handle_ = nullptr;
        return false;
    }
    wake_handle_->data = this;
    return true;
}

void AsyncEventLoop::Run() {
    if (!loop_) return;
    impl_->loop_tid = std::this_thread::get_id();
    impl_->stopping.store(false);
    uv_run(loop_, UV_RUN_DEFAULT);
}

void AsyncEventLoop::Stop() {
    impl_->stopping.store(true);
    if (wake_handle_) {
        uv_async_send(wake_handle_);
    }
}

void AsyncEventLoop::Post(Task task) {
    if (!task) return;

    if (IsInLoopThread()) {
        task();
        return;
    }

    {
        std::lock_guard<std::mutex> lk(impl_->task_mtx);
        impl_->tasks.push_back(std::move(task));
    }
    if (wake_handle_) {
        uv_async_send(wake_handle_);
    }
}

bool AsyncEventLoop::IsInLoopThread() const {
    return std::this_thread::get_id() == impl_->loop_tid;
}

void AsyncEventLoop::OnAsyncWake(uv_async_t* handle) {
    auto* self = static_cast<AsyncEventLoop*>(handle->data);
    if (!self) return;

    std::deque<Task> local;
    {
        std::lock_guard<std::mutex> lk(self->impl_->task_mtx);
        local.swap(self->impl_->tasks);
    }
    for (auto& t : local) {
        if (t) t();
    }

    if (self->impl_->stopping.load()) {
        uv_stop(self->loop_);
    }
}

void AsyncEventLoop::OnWalkClose(uv_handle_t* handle, void* /*arg*/) {
    if (!uv_is_closing(handle)) {
        uv_close(handle, nullptr);
    }
}

} // namespace async
} // namespace net
} // namespace gs
