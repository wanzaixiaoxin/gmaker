#pragma once

#include "../net/tcp_client.hpp"
#include <string>
#include <functional>
#include <atomic>
#include <memory>

// 前向声明：假设 protobuf 已生成
namespace registry {
class NodeInfo;
class NodeId;
class ServiceType;
class Result;
class NodeList;
class NodeEvent;
}

namespace gs {
namespace registry {

constexpr uint32_t CMD_REGISTER  = 0x000F0001;
constexpr uint32_t CMD_HEARTBEAT = 0x000F0002;
constexpr uint32_t CMD_DISCOVER  = 0x000F0003;
constexpr uint32_t CMD_WATCH     = 0x000F0004;
constexpr uint32_t CMD_NODE_EVENT = 0x000F0005;

using EventCallback = std::function<void(const ::registry::NodeEvent&)>;

class RegistryClient {
public:
    explicit RegistryClient(const std::string& host, uint16_t port);
    ~RegistryClient();

    bool Connect();
    void Close();
    bool IsConnected() const;

    // 注册节点（Req-Res 配对待 RPC 层完成后精确实现）
    bool Register(const ::registry::NodeInfo& node, ::registry::Result* out);

    // 心跳
    bool Heartbeat(const std::string& node_id, ::registry::Result* out);

    // 发现节点
    bool Discover(const std::string& service_type, ::registry::NodeList* out);

    // 监听节点变更（建立 Watch 长连接）
    bool Watch(const std::string& service_type, EventCallback on_event);

private:
    void OnPacket(net::TCPConn* conn, net::Packet& pkt);
    void OnClose(net::TCPConn* conn);
    uint32_t NextSeqID();

    std::unique_ptr<net::TCPClient> client_;
    std::atomic<uint32_t> seq_id_{0};
    EventCallback event_cb_;
};

} // namespace registry
} // namespace gs
