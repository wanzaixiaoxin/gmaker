#pragma once

#include "../net/async/upstream.hpp"
#include "../net/async/event_loop.hpp"
#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <future>
#include <chrono>

// 前向声明：假设 protobuf 已生成
namespace registry {
class NodeInfo;
class NodeId;
class ServiceType;
class Result;
class NodeList;
class NodeEvent;
class SubscribeReq;
class SubscribeRes;
}

namespace gs {
namespace registry {

constexpr uint32_t CMD_REGISTER   = 0x000F0001;
constexpr uint32_t CMD_HEARTBEAT  = 0x000F0002;
constexpr uint32_t CMD_DISCOVER   = 0x000F0003;
constexpr uint32_t CMD_WATCH      = 0x000F0004;
constexpr uint32_t CMD_NODE_EVENT = 0x000F0005;
constexpr uint32_t CMD_SUBSCRIBE  = 0x000F0006;

using EventCallback = std::function<void(const ::registry::NodeEvent&)>;

class RegistryClient {
public:
    // loop: 外部事件循环（可为 nullptr，内部将自动创建）
    explicit RegistryClient(net::async::AsyncEventLoop* loop,
                            const std::vector<std::pair<std::string, uint16_t>>& addrs);
    ~RegistryClient();

    bool Connect();
    void Close();
    bool IsConnected() const;

    // 注册节点（Req-Res 同步调用）
    bool Register(const ::registry::NodeInfo& node, ::registry::Result* out);

    // 心跳
    bool Heartbeat(const std::string& node_id, ::registry::Result* out);

    // 发现节点
    bool Discover(const std::string& service_type, ::registry::NodeList* out);

    // 监听节点变更（建立 Watch 长连接，推送 NodeEvent）
    bool Watch(const std::string& service_type, EventCallback on_event);

    // 批量订阅多个服务类型，返回当前全量快照，后续增量通过 on_event 推送
    bool Subscribe(const std::vector<std::string>& service_types,
                   ::registry::SubscribeRes* out_snapshot,
                   EventCallback on_event);

private:
    void OnPacket(net::IConnection* conn, net::Packet& pkt);
    void OnClose(net::IConnection* conn);
    void OnNodeEvent(const std::string& addr, bool healthy);
    uint32_t NextSeqID();

    // 同步 RPC 调用（超时默认 5s）
    bool Call(uint32_t cmd_id, const std::vector<uint8_t>& payload,
              net::Packet* out_pkt, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // 发送 Fire-Forget
    bool FireForget(uint32_t cmd_id, const std::vector<uint8_t>& payload);

    // 重新发送所有 Watch 请求（节点恢复时调用）
    void ResendWatches();

    std::unique_ptr<net::async::AsyncEventLoop> owned_loop_;
    net::async::AsyncEventLoop* loop_ = nullptr;
    std::unique_ptr<net::async::AsyncUpstreamPool> pool_;
    std::atomic<uint32_t> seq_id_{0};

    // Pending RPC 响应
    std::mutex pending_mtx_;
    std::unordered_map<uint32_t, std::promise<net::Packet>> pending_;

    // Watch 状态
    std::mutex watch_mtx_;
    EventCallback event_cb_;
    std::unordered_set<std::string> watch_types_;
};

} // namespace registry
} // namespace gs
