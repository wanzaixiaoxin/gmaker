#include "client.hpp"
// #include <google/protobuf/message.h>
// #include <google/protobuf/util/time_util.h>
#include <iostream>

namespace gs {
namespace registry {

RegistryClient::RegistryClient(const std::string& host, uint16_t port)
    : client_(std::make_unique<net::TCPClient>(host, port)) {
}

RegistryClient::~RegistryClient() {
    Close();
}

bool RegistryClient::Connect() {
    client_->SetCallbacks(
        [this](net::TCPConn* c, net::Packet& p) { OnPacket(c, p); },
        [this](net::TCPConn* c) { OnClose(c); }
    );
    return client_->Connect();
}

void RegistryClient::Close() {
    client_->Close();
}

bool RegistryClient::IsConnected() const {
    return client_->IsConnected();
}

bool RegistryClient::Register(const ::registry::NodeInfo& node, ::registry::Result* out) {
    // TODO: 序列化 NodeInfo，构造 Packet 发送
    // 等待 RPC 层提供 Req-Res 配对后再精确实现
    (void)node;
    (void)out;
    std::cerr << "RegistryClient::Register: skeleton implementation\n";
    return true;
}

bool RegistryClient::Heartbeat(const std::string& node_id, ::registry::Result* out) {
    (void)node_id;
    (void)out;
    std::cerr << "RegistryClient::Heartbeat: skeleton implementation\n";
    return true;
}

bool RegistryClient::Discover(const std::string& service_type, ::registry::NodeList* out) {
    (void)service_type;
    (void)out;
    std::cerr << "RegistryClient::Discover: skeleton implementation\n";
    return true;
}

bool RegistryClient::Watch(const std::string& service_type, EventCallback on_event) {
    (void)service_type;
    event_cb_ = on_event;
    std::cerr << "RegistryClient::Watch: skeleton implementation\n";
    return true;
}

void RegistryClient::OnPacket(net::TCPConn* conn, net::Packet& pkt) {
    (void)conn;
    if (pkt.header.cmd_id == CMD_NODE_EVENT && event_cb_) {
        // ::registry::NodeEvent ev;
        // ev.ParseFromArray(pkt.payload.data(), pkt.payload.size());
        // event_cb_(ev);
    }
}

void RegistryClient::OnClose(net::TCPConn* conn) {
    (void)conn;
}

uint32_t RegistryClient::NextSeqID() {
    return ++seq_id_;
}

} // namespace registry
} // namespace gs
