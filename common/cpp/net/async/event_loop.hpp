#pragma once

#include <functional>
#include <memory>

// 前置声明 libuv 类型
struct uv_loop_s;
struct uv_handle_s;
struct uv_async_s;
using uv_loop_t = uv_loop_s;
using uv_handle_t = uv_handle_s;
using uv_async_t = uv_async_s;

namespace gs {
namespace net {
namespace async {

// AsyncEventLoop 基于 libuv 的事件循环封装
// 提供跨平台的异步 I/O 能力，替代阻塞模型
class AsyncEventLoop {
public:
    using Task = std::function<void()>;

    AsyncEventLoop();
    ~AsyncEventLoop();

    // 禁止拷贝移动
    AsyncEventLoop(const AsyncEventLoop&) = delete;
    AsyncEventLoop& operator=(const AsyncEventLoop&) = delete;

    // 初始化事件循环
    bool Init();

    // 启动事件循环（阻塞直到 Stop）
    void Run();

    // 停止事件循环（线程安全，可从其他线程调用）
    void Stop();

    // 在事件循环线程中执行一个任务（线程安全）
    void Post(Task task);

    // 获取原生 uv_loop_t（供其他组件注册 handle）
    uv_loop_t* RawLoop() const { return loop_; }

    // 判断当前是否在事件循环线程
    bool IsInLoopThread() const;

    // 非阻塞地运行一轮事件循环（仅在 loop 线程调用），返回已处理的事件数
    int PumpOnce();

private:
    static void OnAsyncWake(uv_async_t* handle);
    static void OnWalkClose(uv_handle_t* handle, void* arg);

    uv_loop_t* loop_ = nullptr;
    uv_async_t* wake_handle_ = nullptr;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace async
} // namespace net
} // namespace gs
