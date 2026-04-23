#pragma once

#include "client.hpp"
#include "../net/async/upstream.hpp"
#include "../net/async/event_loop.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

// 前向声明
namespace registry {
class SubscribeReq;
class SubscribeRes;
class NodeEvent;
}

namespace gs {
namespace registry {

// UpstreamManager 上游服务管理器
// 封装了向 Registry 批量订阅、初始化连接池、动态增删节点的完整逻辑。
// 使用方式：
//   1. 构造时传入 RegistryClient（需已连接）
//   2. 为每种关注的服务调用 AddInterest
//   3. 调用 Start() 启动订阅与连接池
//   4. 通过 GetPool(service_type) 获取连接池发送数据
//   5. 服务退出时调用 Stop()
class UpstreamManager {
public:
    // loop: 外部事件循环（各连接池共享此循环，可为 nullptr）
    explicit UpstreamManager(RegistryClient* client,
                             net::async::AsyncEventLoop* loop);
    ~UpstreamManager();

    // 声明对某类上游服务的兴趣
    // on_data: 收到该上游回包时的回调
    // 返回对应的连接池（尚未启动，Start 后才会真正连接）
    net::async::AsyncUpstreamPool* AddInterest(
        const std::string& service_type,
        net::async::AsyncUpstreamPool::DataCallback on_data);

    // 向 Registry 批量订阅所有关注的服务类型，
    // 收到全量快照后初始化各连接池节点，并启动连接池。
    bool Start();

    // 停止所有连接池
    void Stop();

    // 获取指定服务类型的连接池
    net::async::AsyncUpstreamPool* GetPool(const std::string& service_type);

private:
    void OnNodeEvent(const ::registry::NodeEvent& ev);

    RegistryClient* client_ = nullptr;
    net::async::AsyncEventLoop* loop_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<net::async::AsyncUpstreamPool>> pools_;
    std::mutex mtx_;
};

} // namespace registry
} // namespace gs
