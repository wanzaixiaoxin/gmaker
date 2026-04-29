#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <cstring>
#include "gateway_config.hpp"
#include "net/async/ws_server.hpp"
#include "net/async/tcp_server.hpp"
#include "net/async/upstream.hpp"
#include "net/async/coalescer.hpp"
#include "net/packet.hpp"
#include "net/address.hpp"
#include "crypto/session.hpp"
#include "replay/replay.hpp"
#include "discovery/factory.hpp"
#include "discovery/upstream_manager.hpp"
#include "metrics/metrics.hpp"
#include "errors.hpp"
#include "logger/logger.hpp"
#include "registry.pb.h"
#include "protocol.pb.h"  // 命令 ID 定义

using namespace gs::net;
using namespace gs::net::async;
using namespace gs::crypto;

std::string ToHex(uint32_t v);

bool DecodeClientPacket(const Buffer& data, Packet& pkt);
std::vector<uint8_t> EncodeCommonResultPayload(uint32_t code, const std::string& msg);

// HandshakeMiddleware 处理 Gateway-Client 握手
class HandshakeMiddleware : public Middleware {
public:
    HandshakeMiddleware(const std::vector<uint8_t>& master_key,
                        gs::replay::Checker* replay_checker,
                        std::shared_ptr<gs::logger::Logger> logger,
                        uint32_t cmd_handshake)
        : master_key_(master_key), replay_checker_(replay_checker), 
          logger_(std::move(logger)), cmd_handshake_(cmd_handshake) {}

    bool OnPacket(IConnection* conn, Packet& pkt) override {
        if (pkt.header.cmd_id != cmd_handshake_) {
            return true;
        }
        HandleHandshake(conn, pkt);
        return false;
    }

    bool IsHandshaked(uint64_t conn_id) const {
        std::lock_guard<std::mutex> lk(mtx_);
        return sessions_.find(conn_id) != sessions_.end();
    }

    std::vector<uint8_t> GetSessionKey(uint64_t conn_id) const {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = sessions_.find(conn_id);
        if (it != sessions_.end()) return it->second;
        return {};
    }

    void RemoveSession(uint64_t conn_id) {
        std::lock_guard<std::mutex> lk(mtx_);
        sessions_.erase(conn_id);
    }

private:
    void HandleHandshake(IConnection* conn, Packet& pkt);
    
    std::vector<uint8_t> master_key_;
    gs::replay::Checker* replay_checker_;
    std::shared_ptr<gs::logger::Logger> logger_;
    uint32_t cmd_handshake_;
    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> sessions_;
};

// EncryptionMiddleware 处理 Gateway-Client 加密/解密
class EncryptionMiddleware : public Middleware {
public:
    EncryptionMiddleware(HandshakeMiddleware* handshake_mw, std::shared_ptr<gs::logger::Logger> logger)
        : handshake_mw_(handshake_mw), logger_(std::move(logger)) {}

    bool OnPacket(IConnection* conn, Packet& pkt) override;

private:
    HandshakeMiddleware* handshake_mw_;
    std::shared_ptr<gs::logger::Logger> logger_;
};

class WebSocketPacketConnection : public IConnection {
public:
    explicit WebSocketPacketConnection(gs::net::websocket::WebSocketConnection* ws)
        : ws_(ws), id_(ws ? (ws->ID() | (1ull << 63)) : 0) {}

    uint64_t ID() const override { return id_; }

    void Close() override {
        auto* ws = ws_.load();
        if (ws) ws->Close();
    }

    void CloseAfterWrite() override {
        auto* ws = ws_.load();
        if (ws) ws->CloseAfterWrite();
    }

    bool SendPacket(const Packet& pkt) override {
        auto data = EncodePacket(pkt);
        return Send(data);
    }

    bool Send(std::vector<uint8_t> data) override {
        return Send(Buffer::FromVector(std::move(data)));
    }

    bool Send(const Buffer& data) override {
        auto* ws = ws_.load();
        return ws && ws->SendMessage(gs::net::websocket::MessageType::Binary, data);
    }

    bool SendBatch(const std::vector<Buffer>& buffers) override {
        auto* ws = ws_.load();
        return ws && ws->SendMessageBatch(gs::net::websocket::MessageType::Binary, buffers);
    }

    void Detach() {
        ws_.store(nullptr);
    }

private:
    std::atomic<gs::net::websocket::WebSocketConnection*> ws_{nullptr};
    uint64_t id_ = 0;
};

// Gateway 主类
class Gateway {
public:
    void SetLogger(std::shared_ptr<gs::logger::Logger> logger) { logger_ = logger; }

    bool Start(const gs::gateway::Config& cfg);
    void Stop();
    void Wait();

private:
    void OnClientConnect(IConnection* conn);
    void OnClientPacket(IConnection* conn, Packet& pkt);
    void OnClientClose(IConnection* conn);
    void OnWebSocketConnect(gs::net::websocket::WebSocketConnection* conn);
    void OnWebSocketMessage(gs::net::websocket::WebSocketConnection* conn,
                            gs::net::websocket::MessageType type,
                            Buffer& message);
    void OnWebSocketClose(gs::net::websocket::WebSocketConnection* conn);
    void OnUpstreamPacket(IConnection* conn, Packet& pkt);
    
    // 根据命令 ID 路由到对应的上游池
    AsyncUpstreamPool* RouteToPool(uint32_t cmd_id);
    
    void HeartbeatLoop();
    
    std::unique_ptr<AsyncTCPServer> server_;
    std::unique_ptr<gs::net::websocket::WebSocketServer> ws_server_;
    std::unique_ptr<gs::discovery::UpstreamManager> upstream_mgr_;
    std::unique_ptr<AsyncWriteCoalescer> coalescer_;
    std::unique_ptr<gs::discovery::ServiceDiscovery> sd_;

    std::mutex sessions_mtx_;
    std::unordered_map<uint64_t, IConnection*> clients_;
    std::unordered_map<uint64_t, std::shared_ptr<WebSocketPacketConnection>> ws_clients_;
    std::vector<uint8_t> master_key_;
    std::unique_ptr<gs::replay::Checker> replay_checker_;
    
    std::shared_ptr<gs::logger::Logger> logger_;
    std::shared_ptr<HandshakeMiddleware> handshake_mw_;
    std::shared_ptr<EncryptionMiddleware> encryption_mw_;
    
    // Room 管理
    std::mutex room_mtx_;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> room_members_;
    std::unordered_map<uint64_t, uint64_t> conn_room_;
    
    // Player 绑定管理（单设备登录 / 顶号）
    std::mutex player_mtx_;
    std::unordered_map<uint64_t, uint64_t> player_conn_;   // player_id → conn_id
    std::unordered_map<uint64_t, uint64_t> conn_player_;   // conn_id → player_id
    struct PendingBind {
        uint64_t player_id = 0;
        uint32_t seq_id = 0;
        std::chrono::steady_clock::time_point created_at;
    };
    std::unordered_map<uint64_t, PendingBind> pending_binds_; // conn_id → latest bind request
    
    void HandlePlayerBind(IConnection* conn, Packet& pkt);
    void BindPlayer(uint64_t player_id, uint64_t conn_id);
    void KickConnection(uint64_t conn_id, const std::string& reason);
    void CleanupPlayerState(uint64_t conn_id);
    void SendToClientDirect(uint64_t conn_id, const Packet& pkt);
    void SendToClientDirect(uint64_t conn_id, const Packet& pkt, bool close_after_write);
    void SendErrorAndClose(uint64_t conn_id, uint32_t seq_id, uint32_t code, const std::string& msg);
    bool IsPlayerBound(uint64_t conn_id);
    
    // 配置
    gs::gateway::Config cfg_;
    
    // Metrics
    gs::metrics::Counter*   req_counter_ = nullptr;
    gs::metrics::Histogram* req_latency_ = nullptr;
    gs::metrics::Gauge*     conn_gauge_  = nullptr;
    
    // 心跳线程
    std::thread heartbeat_thread_;
    std::atomic<bool> heartbeat_stop_{false};
    
    std::mutex stop_mtx_;
    std::condition_variable stop_cv_;
    bool stop_flag_ = false;
};

// ==================== 实现 ====================

void HandshakeMiddleware::HandleHandshake(IConnection* conn, Packet& pkt) {
    constexpr size_t kHandshakeV1Size = 1 + 8 + 8 + 16;
    if (pkt.payload.Size() < kHandshakeV1Size) {
        if (logger_) logger_->Error("Invalid handshake request from client " + std::to_string(conn->ID()));
        conn->Close();
        return;
    }

    uint8_t version = pkt.payload[0];
    if (version != 1) {
        if (logger_) logger_->Error("Unsupported handshake version: " + std::to_string((int)version));
        conn->Close();
        return;
    }

    uint64_t timestamp = ReadU64BE(pkt.payload.Data() + 1);
    std::string nonce(pkt.payload.Data() + 9, pkt.payload.Data() + 17);
    std::vector<uint8_t> client_random(pkt.payload.Data() + 17, pkt.payload.Data() + 33);

    auto ts = std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
    if (!replay_checker_->Check(ts, nonce)) {
        if (logger_) logger_->Error("Replay detected or invalid timestamp from client " + std::to_string(conn->ID()));
        conn->Close();
        return;
    }

    std::vector<uint8_t> server_random = RandomBytes(16);
    auto session_key = DeriveSessionKey(master_key_, client_random, server_random);
    
    {
        std::lock_guard<std::mutex> lk(mtx_);
        sessions_[conn->ID()] = session_key;
    }

    // 发送握手响应: [version: 1][server_random: 16][encrypted_challenge: ...]
    auto encrypted_challenge = EncryptPacketPayload(session_key, client_random);

    Packet res;
    res.header.magic = MAGIC_VALUE;
    res.header.cmd_id = cmd_handshake_;
    res.header.seq_id = pkt.header.seq_id;
    res.header.flags = 0;
    
    std::vector<uint8_t> payload;
    payload.reserve(1 + 16 + encrypted_challenge.size());
    payload.push_back(1); // version
    payload.insert(payload.end(), server_random.begin(), server_random.end());
    payload.insert(payload.end(), encrypted_challenge.begin(), encrypted_challenge.end());
    
    res.payload = Buffer::FromVector(payload);
    res.header.length = HEADER_SIZE + static_cast<uint32_t>(res.payload.Size());
    
    conn->SendPacket(res);
    if (logger_) logger_->Info("Handshake completed for client " + std::to_string(conn->ID()));
}

bool EncryptionMiddleware::OnPacket(IConnection* conn, Packet& pkt) {
    if (!handshake_mw_->IsHandshaked(conn->ID())) {
        if (logger_) logger_->Error("Client " + std::to_string(conn->ID()) + " sent packet before handshake, closing");
        conn->Close();
        return false;
    }

    auto session_key = handshake_mw_->GetSessionKey(conn->ID());
    if (session_key.empty()) {
        if (logger_) logger_->Error("No session key for client " + std::to_string(conn->ID()));
        conn->Close();
        return false;
    }

    if (pkt.payload.Size() == 0) return true;

    try {
        auto decrypted = DecryptPacketPayload(session_key, 
            std::vector<uint8_t>(pkt.payload.Data(), pkt.payload.Data() + pkt.payload.Size()));
        pkt.payload = Buffer::FromVector(decrypted);
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("Decryption failed for client " + std::to_string(conn->ID()) + ": " + e.what());
        conn->Close();
        return false;
    }

    return true;
}

bool DecodeClientPacket(const Buffer& data, Packet& pkt) {
    if (data.Size() < HEADER_SIZE || data.Size() > MAX_PACKET_LEN) {
        return false;
    }
    const uint8_t* p = data.Data();
    if (!p) return false;

    Header h = DecodeHeader(p);
    if (h.magic != MAGIC_VALUE || h.length != data.Size()) {
        return false;
    }
    if (h.length < HEADER_SIZE || h.length > MAX_PACKET_LEN) {
        return false;
    }

    pkt.header = h;
    size_t payload_len = data.Size() - HEADER_SIZE;
    if (payload_len > 0) {
        pkt.payload = Buffer::Allocate(payload_len);
        std::memcpy(pkt.payload.Data(), p + HEADER_SIZE, payload_len);
    } else {
        pkt.payload = Buffer();
    }
    return true;
}

namespace {

void AppendVarint(std::vector<uint8_t>& out, uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(value) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

} // namespace

std::vector<uint8_t> EncodeCommonResultPayload(uint32_t code, const std::string& msg) {
    std::vector<uint8_t> out;
    out.reserve(8 + msg.size());
    AppendVarint(out, (2u << 3) | 0u); // common.Result.code
    AppendVarint(out, code);
    if (!msg.empty()) {
        AppendVarint(out, (3u << 3) | 2u); // common.Result.msg
        AppendVarint(out, msg.size());
        out.insert(out.end(), msg.begin(), msg.end());
    }
    return out;
}

bool Gateway::Start(const gs::gateway::Config& cfg) {
    cfg_ = cfg;
    master_key_ = gs::gateway::ParseMasterKey(cfg);
    replay_checker_ = std::make_unique<gs::replay::Checker>(std::chrono::seconds(cfg.replay_window_seconds));

    // 启动 metrics
    gs::metrics::ServeDefaultHTTP(cfg.metrics_addr);
    req_counter_ = gs::metrics::DefaultCounter("gateway_requests_total");
    req_latency_ = gs::metrics::DefaultHistogram("gateway_request_duration_ms", {1, 5, 10, 25, 50, 100, 250, 500, 1000});
    conn_gauge_  = gs::metrics::DefaultGauge("gateway_connections");

    // 启动 TCP 服务器
    AsyncTCPServer::Config server_cfg;
    server_cfg.port = cfg.listen_port;
    server_cfg.max_conn = cfg.max_connections;
    server_ = std::make_unique<AsyncTCPServer>(server_cfg);

    // 初始化中间件
    handshake_mw_ = std::make_shared<HandshakeMiddleware>(master_key_, replay_checker_.get(), logger_, protocol::CMD_SYS_HANDSHAKE);
    encryption_mw_ = std::make_shared<EncryptionMiddleware>(handshake_mw_.get(), logger_);
    
    server_->SetCallbacks(
        [this](AsyncTCPConnection* c) { OnClientConnect(c); },
        [this](AsyncTCPConnection* c, Packet& p) { OnClientPacket(c, p); },
        [this](AsyncTCPConnection* c) { OnClientClose(c); }
    );
    server_->Use(handshake_mw_);
    server_->Use(encryption_mw_);

    if (!server_->Start()) {
        if (logger_) logger_->Error("Failed to start gateway server");
        return false;
    }
    if (logger_) logger_->Info("Gateway TCP server started on port " + std::to_string(cfg.listen_port));

    // 启动 WebSocket 服务器（如果配置端口 > 0）
    if (cfg.websocket_port > 0) {
        gs::net::websocket::WebSocketServer::Config ws_cfg;
        ws_cfg.port = cfg.websocket_port;
        ws_cfg.max_conn = cfg.max_connections;
        ws_server_ = std::make_unique<gs::net::websocket::WebSocketServer>(ws_cfg);
        ws_server_->SetCallbacks(
            [this](gs::net::websocket::WebSocketConnection* c) { OnWebSocketConnect(c); },
            [this](gs::net::websocket::WebSocketConnection* c,
                   gs::net::websocket::MessageType t,
                   Buffer& m) { OnWebSocketMessage(c, t, m); },
            [this](gs::net::websocket::WebSocketConnection* c) { OnWebSocketClose(c); }
        );
        if (!ws_server_->Start()) {
            if (logger_) logger_->Warn("Failed to start WebSocket server on port " + std::to_string(cfg.websocket_port));
        } else {
            if (logger_) logger_->Info("Gateway WebSocket server started on port " + std::to_string(cfg.websocket_port));
        }
    }

    // 初始化写聚合器
    coalescer_ = std::make_unique<AsyncWriteCoalescer>(server_->EventLoop(), cfg.coalescer_interval_ms);
    if (!coalescer_->Start()) {
        if (logger_) logger_->Error("Failed to start write coalescer");
        return false;
    }

    // 连接服务发现后端并管理上游连接池
    if (!cfg.discovery_addrs.empty()) {
        sd_ = gs::discovery::CreateDiscovery(cfg.discovery_type, cfg.discovery_addrs);
        
        gs::discovery::NodeInfo node;
        node.service_type = "gateway";
        node.node_id = cfg.node_id;
        node.host = "127.0.0.1";
        node.port = cfg.listen_port;
        node.register_at = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        if (sd_->Register(node)) {
            if (logger_) logger_->Info("Gateway registered to registry");
        } else {
            if (logger_) logger_->Warn("Gateway register to registry failed");
        }
        
        upstream_mgr_ = std::make_unique<gs::discovery::UpstreamManager>(sd_.get(), server_->EventLoop());
        
        // 声明对所有配置上游服务的兴趣
        for (const auto& svc_type : cfg.upstream_services) {
            upstream_mgr_->AddInterest(svc_type,
                [this](IConnection* c, Packet& p) { OnUpstreamPacket(c, p); });
        }
        
        // 批量订阅：一次性获取全量快照 + 后续增量推送
        if (!upstream_mgr_->Start()) {
            if (logger_) logger_->Error("UpstreamManager start failed, exiting");
            return false;
        }
        for (const auto& svc_type : cfg.upstream_services) {
            auto* pool = upstream_mgr_->GetPool(svc_type);
            if (pool && logger_) {
                logger_->Info(svc_type + " upstream pool started, " +
                    std::to_string(pool->HealthyCount()) + "/" + std::to_string(pool->TotalCount()) + " nodes");
            }
        }
    }

    heartbeat_stop_.store(false);
    heartbeat_thread_ = std::thread([this]() { HeartbeatLoop(); });

    return true;
}

void Gateway::Stop() {
    heartbeat_stop_.store(true);
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    
    if (server_) server_->Stop();
    if (ws_server_) ws_server_->Stop();
    if (coalescer_) coalescer_->Stop();
    if (upstream_mgr_) upstream_mgr_->Stop();
    if (sd_) sd_->Close();
    
    std::lock_guard<std::mutex> lk(stop_mtx_);
    stop_flag_ = true;
    stop_cv_.notify_all();
}

void Gateway::HeartbeatLoop() {
    const auto kPendingBindTimeout = std::chrono::seconds(10);
    const auto kScanInterval = std::chrono::seconds(5);

    while (!heartbeat_stop_.load()) {
        std::this_thread::sleep_for(kScanInterval);

        // 清理超时的 pending_bind 记录
        auto now = std::chrono::steady_clock::now();
        std::vector<uint64_t> expired_conns;
        {
            std::lock_guard<std::mutex> lk(player_mtx_);
            for (const auto& [conn_id, info] : pending_binds_) {
                if (now - info.created_at > kPendingBindTimeout) {
                    expired_conns.push_back(conn_id);
                }
            }
            for (auto conn_id : expired_conns) {
                pending_binds_.erase(conn_id);
            }
        }
        if (!expired_conns.empty() && logger_) {
            logger_->Info("Cleaned up " + std::to_string(expired_conns.size()) + " expired pending binds");
        }
    }
}

void Gateway::Wait() {
    std::unique_lock<std::mutex> lk(stop_mtx_);
    stop_cv_.wait(lk, [this] { return stop_flag_; });
}

void Gateway::OnClientConnect(IConnection* conn) {
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    clients_[conn->ID()] = conn;
    conn_gauge_->Inc();
    if (logger_) logger_->Info("Client connected: " + std::to_string(conn->ID()));
}

void Gateway::OnWebSocketConnect(gs::net::websocket::WebSocketConnection* conn) {
    auto adapter = std::make_shared<WebSocketPacketConnection>(conn);
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        ws_clients_[adapter->ID()] = adapter;
        clients_[adapter->ID()] = adapter.get();
    }
    conn_gauge_->Inc();
    if (logger_) logger_->Info("Client connected: " + std::to_string(adapter->ID()));
}

void Gateway::OnWebSocketMessage(gs::net::websocket::WebSocketConnection* conn,
                                 gs::net::websocket::MessageType type,
                                 Buffer& message) {
    if (type != gs::net::websocket::MessageType::Binary) {
        conn->Close();
        return;
    }

    std::shared_ptr<WebSocketPacketConnection> adapter;
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        uint64_t app_conn_id = conn->ID() | (1ull << 63);
        auto it = ws_clients_.find(app_conn_id);
        if (it != ws_clients_.end()) {
            adapter = it->second;
        }
    }
    if (!adapter) {
        conn->Close();
        return;
    }

    Packet pkt;
    if (!DecodeClientPacket(message, pkt)) {
        if (logger_) logger_->Warn("Invalid WebSocket packet from client " + std::to_string(conn->ID()));
        conn->Close();
        return;
    }

    if (pkt.header.cmd_id == protocol::CMD_SYS_HANDSHAKE) {
        if (!handshake_mw_->OnPacket(adapter.get(), pkt)) {
            return;
        }
    }
    if (!encryption_mw_->OnPacket(adapter.get(), pkt)) {
        return;
    }
    OnClientPacket(adapter.get(), pkt);
}

void Gateway::OnWebSocketClose(gs::net::websocket::WebSocketConnection* conn) {
    std::shared_ptr<WebSocketPacketConnection> adapter;
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        uint64_t app_conn_id = conn->ID() | (1ull << 63);
        auto it = ws_clients_.find(app_conn_id);
        if (it != ws_clients_.end()) {
            adapter = it->second;
            ws_clients_.erase(it);
        }
    }
    if (!adapter) return;
    adapter->Detach();

    {
        std::lock_guard<std::mutex> lk(room_mtx_);
        auto it = conn_room_.find(adapter->ID());
        if (it != conn_room_.end()) {
            auto rit = room_members_.find(it->second);
            if (rit != room_members_.end()) {
                rit->second.erase(adapter->ID());
                if (rit->second.empty()) room_members_.erase(rit);
            }
            conn_room_.erase(it);
        }
    }

    // 清理 player 绑定状态
    CleanupPlayerState(adapter->ID());

    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        clients_.erase(adapter->ID());
        handshake_mw_->RemoveSession(adapter->ID());
    }
    conn_gauge_->Dec();
    if (logger_) logger_->Info("Client disconnected: " + std::to_string(adapter->ID()));
}

void Gateway::OnClientPacket(IConnection* conn, Packet& pkt) {
    auto start = std::chrono::steady_clock::now();
    req_counter_->Inc();

    if (logger_) logger_->Info("Forwarding packet: cmd=0x" + ToHex(pkt.header.cmd_id) + 
                               " seq=" + std::to_string(pkt.header.seq_id) + 
                               " conn=" + std::to_string(conn->ID()));

    // 拦截玩家绑定请求（Gateway 内部命令，由 Biz 验证 Token）
    if (pkt.header.cmd_id == protocol::CMD_GW_PLAYER_BIND) {
        HandlePlayerBind(conn, pkt);
        auto elapsed = std::chrono::steady_clock::now() - start;
        req_latency_->Observe(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
        return;
    }

    bool is_system_cmd = pkt.header.cmd_id <= 0x000000FF;
    if (!is_system_cmd && !IsPlayerBound(conn->ID())) {
        if (logger_) logger_->Warn("Reject unbound client packet: cmd=0x" + ToHex(pkt.header.cmd_id) +
                                   " conn=" + std::to_string(conn->ID()));
        SendErrorAndClose(conn->ID(), pkt.header.seq_id, gs::errors::AUTH_FAILED, "player not bound");
        auto elapsed = std::chrono::steady_clock::now() - start;
        req_latency_->Observe(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
        return;
    }

    // 根据命令 ID 路由到对应的上游池
    auto pool = RouteToPool(pkt.header.cmd_id);
    if (!pool) {
        if (logger_) logger_->Warn("No upstream pool for cmd=0x" + ToHex(pkt.header.cmd_id));
        return;
    }

    // 转发到上游
    pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size());
    
    // 附加 conn_id 用于响应路由
    std::vector<uint8_t> extended_payload(pkt.payload.Size() + 8);
    WriteU64BE(extended_payload.data(), conn->ID());
    if (pkt.payload.Size() > 0) {
        std::memcpy(extended_payload.data() + 8, pkt.payload.Data(), pkt.payload.Size());
    }
    pkt.payload = Buffer(std::move(extended_payload));
    pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size());

    if (!pool->SendPacket(pkt)) {
        if (logger_) logger_->Warn("Failed to forward packet to upstream, cmd=0x" + ToHex(pkt.header.cmd_id));
    } else {
        if (logger_) logger_->Info("Packet forwarded to upstream: cmd=0x" + ToHex(pkt.header.cmd_id));
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    req_latency_->Observe(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
}

void Gateway::OnClientClose(IConnection* conn) {
    // 清理 room 成员
    {
        std::lock_guard<std::mutex> lk(room_mtx_);
        auto it = conn_room_.find(conn->ID());
        if (it != conn_room_.end()) {
            auto rit = room_members_.find(it->second);
            if (rit != room_members_.end()) {
                rit->second.erase(conn->ID());
                if (rit->second.empty()) room_members_.erase(rit);
            }
            conn_room_.erase(it);
        }
    }
    
    // 清理 player 绑定状态
    CleanupPlayerState(conn->ID());
    
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    clients_.erase(conn->ID());
    handshake_mw_->RemoveSession(conn->ID());
    conn_gauge_->Dec();
    if (logger_) logger_->Info("Client disconnected: " + std::to_string(conn->ID()));
}

void Gateway::OnUpstreamPacket(IConnection* conn, Packet& pkt) {
    if (logger_) logger_->Info("Received from upstream: cmd=0x" + ToHex(pkt.header.cmd_id) + 
                               " seq=" + std::to_string(pkt.header.seq_id) + 
                               " flags=0x" + ToHex(pkt.header.flags) +
                               " payload=" + std::to_string(pkt.payload.Size()));

    // 处理 Gateway 内部控制协议
    if (pkt.header.cmd_id == protocol::CMD_GW_ROOM_JOIN) {
        if (pkt.payload.Size() >= 16) {
            uint64_t room_id = ReadU64BE(pkt.payload.Data());
            uint64_t conn_id = ReadU64BE(pkt.payload.Data() + 8);
            std::lock_guard<std::mutex> lk(room_mtx_);
            room_members_[room_id].insert(conn_id);
            conn_room_[conn_id] = room_id;
            if (logger_) logger_->Info("Room JOIN: room=" + std::to_string(room_id) + " conn=" + std::to_string(conn_id));
        }
        return;
    }

    if (pkt.header.cmd_id == protocol::CMD_GW_ROOM_LEAVE) {
        if (pkt.payload.Size() >= 16) {
            uint64_t room_id = ReadU64BE(pkt.payload.Data());
            uint64_t conn_id = ReadU64BE(pkt.payload.Data() + 8);
            std::lock_guard<std::mutex> lk(room_mtx_);
            auto it = room_members_.find(room_id);
            if (it != room_members_.end()) {
                it->second.erase(conn_id);
                if (it->second.empty()) room_members_.erase(it);
            }
            conn_room_.erase(conn_id);
            if (logger_) logger_->Info("Room LEAVE: room=" + std::to_string(room_id) + " conn=" + std::to_string(conn_id));
        }
        return;
    }

    // 处理玩家绑定响应（Biz 验证 Token 后返回）
    if (pkt.header.cmd_id == protocol::CMD_GW_PLAYER_BIND) {
        bool should_close_bind_conn = false;
        bool should_forward_bind_response = false;
        uint64_t bind_conn_id = 0;
        if (pkt.payload.Size() >= 8) {
            uint64_t conn_id = ReadU64BE(pkt.payload.Data());
            bind_conn_id = conn_id;
            protocol::PlayerBindRes res;
            if (res.ParseFromArray(pkt.payload.Data() + 8, pkt.payload.Size() - 8)) {
                uint64_t player_id = 0;
                {
                    std::lock_guard<std::mutex> lk(player_mtx_);
                    auto it = pending_binds_.find(conn_id);
                    if (it != pending_binds_.end() && it->second.seq_id == pkt.header.seq_id) {
                        player_id = it->second.player_id;
                        pending_binds_.erase(it);
                        should_forward_bind_response = true;
                    }
                }

                if (!should_forward_bind_response) {
                    if (logger_) logger_->Warn("Drop stale PlayerBind response: conn=" + std::to_string(conn_id) +
                                                " seq=" + std::to_string(pkt.header.seq_id));
                    return;
                }

                if (res.code() == 0 && player_id > 0) {
                    BindPlayer(player_id, conn_id);
                } else if (res.code() != 0) {
                    should_close_bind_conn = true;
                    if (logger_) logger_->Info("PlayerBind failed: conn=" + std::to_string(conn_id) + " msg=" + res.msg());
                }
            } else {
                std::lock_guard<std::mutex> lk(player_mtx_);
                auto it = pending_binds_.find(conn_id);
                if (it != pending_binds_.end() && it->second.seq_id == pkt.header.seq_id) {
                    pending_binds_.erase(it);
                }
                if (logger_) logger_->Warn("PlayerBind response parse failed: conn=" + std::to_string(conn_id));
                return;
            }
        }
        // 继续转发响应给客户端（使用下面的通用响应转发逻辑）
        if (should_close_bind_conn) {
            if (bind_conn_id != 0) {
                Packet response;
                response.header = pkt.header;
                response.header.flags &= ~static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
                response.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size() - 8);
                if (pkt.payload.Size() > 8) {
                    response.payload = Buffer(std::vector<uint8_t>(pkt.payload.Data() + 8,
                                                                   pkt.payload.Data() + pkt.payload.Size()));
                }
                SendToClientDirect(bind_conn_id, response, true);
            }
            return;
        }
    }

    // 普通响应：广播或单播
    if (pkt.payload.Size() < 8) return;
    
    bool is_room_bcast = (pkt.header.flags & static_cast<uint32_t>(gs::net::Flag::ROOM_BCAST)) != 0;
    uint64_t conn_id = ReadU64BE(pkt.payload.Data());
    
    if (logger_) logger_->Info("Processing upstream response: is_bcast=" + std::to_string(is_room_bcast) + 
                               " conn_id=" + std::to_string(conn_id));
    
    // 检查是否为房间广播
    std::vector<uint64_t> targets;
    if (is_room_bcast) {
        uint64_t room_id = conn_id; // payload 前 8 字节为 room_id
        std::lock_guard<std::mutex> lk(room_mtx_);
        auto rit = room_members_.find(room_id);
        if (rit != room_members_.end()) {
            targets.assign(rit->second.begin(), rit->second.end());
            if (logger_) logger_->Info("Room broadcast: room=" + std::to_string(room_id) + 
                                       " members=" + std::to_string(targets.size()));
        } else {
            if (logger_) logger_->Warn("Room broadcast: room=" + std::to_string(room_id) + " not found in room_members_");
        }
    } else {
        std::lock_guard<std::mutex> lk(room_mtx_);
        auto it = conn_room_.find(conn_id);
        if (it != conn_room_.end()) {
            auto rit = room_members_.find(it->second);
            if (rit != room_members_.end()) {
                targets.assign(rit->second.begin(), rit->second.end());
                if (logger_) logger_->Info("Unicast to room: conn=" + std::to_string(conn_id) + 
                                           " room=" + std::to_string(it->second) + " members=" + std::to_string(targets.size()));
            }
        } else {
            if (logger_) logger_->Warn("Unicast: conn=" + std::to_string(conn_id) + " not found in conn_room_");
        }
    }

    // 准备响应包模板：payload 前 8 字节是目标 conn_id，不下发给客户端。
    Packet plain_res;
    plain_res.header = pkt.header;
    plain_res.header.flags &= ~static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
    plain_res.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size() - 8);
    if (pkt.payload.Size() > 8) {
        plain_res.payload = Buffer(std::vector<uint8_t>(pkt.payload.Data() + 8, pkt.payload.Data() + pkt.payload.Size()));
    }

    auto build_response_for = [&](uint64_t target_id, Packet& out) -> bool {
        out = plain_res;
        if (out.payload.Size() == 0) {
            out.header.length = HEADER_SIZE;
            return true;
        }

        auto session_key = handshake_mw_->GetSessionKey(target_id);
        if (session_key.empty()) {
            if (logger_) logger_->Warn("No session key for response target " + std::to_string(target_id));
            return false;
        }

        try {
            auto encrypted = EncryptPacketPayload(session_key,
                std::vector<uint8_t>(out.payload.Data(), out.payload.Data() + out.payload.Size()));
            out.payload = Buffer::FromVector(std::move(encrypted));
            out.header.length = HEADER_SIZE + static_cast<uint32_t>(out.payload.Size());
            out.header.flags |= static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
            return true;
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Encryption failed for target " + std::to_string(target_id) + ": " + std::string(e.what()));
            return false;
        }
    };

    auto enqueue_to_client = [&](uint64_t target_id, const Packet& response) {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        auto it = clients_.find(target_id);
        if (it == clients_.end()) return;

        auto ws_it = ws_clients_.find(target_id);
        if (ws_it != ws_clients_.end()) {
            coalescer_->Enqueue(std::static_pointer_cast<IConnection>(ws_it->second), response);
        } else {
            coalescer_->Enqueue(it->second, response);
        }
    };

    if (targets.empty()) {
        Packet response;
        if (build_response_for(conn_id, response)) {
            enqueue_to_client(conn_id, response);
            if (logger_) logger_->Info("Response sent to client: conn=" + std::to_string(conn_id) + 
                                       " cmd=0x" + ToHex(response.header.cmd_id));
        } else {
            if (logger_) logger_->Warn("Failed to build response for client: conn=" + std::to_string(conn_id));
        }
    } else {
        for (auto cid : targets) {
            Packet response;
            if (build_response_for(cid, response)) {
                enqueue_to_client(cid, response);
            }
        }
        if (logger_) logger_->Info("Room broadcast response sent: targets=" + std::to_string(targets.size()));
    }
}

AsyncUpstreamPool* Gateway::RouteToPool(uint32_t cmd_id) {
    // 根据命令 ID 查找服务类型（按范围精确匹配，避免重叠）
    std::string svc_type;
    
    if (cmd_id >= 0x00010000 && cmd_id <= 0x0001FFFF) {
        svc_type = "biz";
    } else if (cmd_id >= 0x00001000 && cmd_id <= 0x0000FFFF) {
        // Common commands: login/register are now handled by HTTP login service;
        // any remaining common-range packets go to biz.
        svc_type = "biz";
    } else if (cmd_id >= 0x00020000 && cmd_id <= 0x0002FFFF) {
        svc_type = "realtime";
    } else if (cmd_id >= 0x00030000 && cmd_id <= 0x0003FFFF) {
        svc_type = "chat";
    } else if (cmd_id >= 0x00040000 && cmd_id <= 0x0004FFFF) {
        svc_type = "logstats";
    }
    
    // 通过 UpstreamManager 获取连接池
    if (!svc_type.empty() && upstream_mgr_) {
        auto* pool = upstream_mgr_->GetPool(svc_type);
        if (pool) return pool;
    }
    
    // 默认路由到第一个服务
    if (!cfg_.upstream_services.empty() && upstream_mgr_) {
        return upstream_mgr_->GetPool(cfg_.upstream_services[0]);
    }
    
    return nullptr;
}

std::string ToHex(uint32_t v) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << v;
    return ss.str();
}

// ==================== Player 绑定管理 ====================

void Gateway::HandlePlayerBind(IConnection* conn, Packet& pkt) {
    protocol::PlayerBindReq req;
    if (!req.ParseFromArray(pkt.payload.Data(), static_cast<int>(pkt.payload.Size()))) {
        if (logger_) logger_->Warn("Invalid PlayerBindReq from client " + std::to_string(conn->ID()));
        SendErrorAndClose(conn->ID(), pkt.header.seq_id, gs::errors::INVALID_PARAM, "invalid bind request");
        return;
    }

    uint64_t player_id = req.player_id();
    if (player_id == 0 || req.token().empty()) {
        if (logger_) logger_->Warn("Invalid PlayerBindReq fields from client " + std::to_string(conn->ID()));
        SendErrorAndClose(conn->ID(), pkt.header.seq_id, gs::errors::INVALID_PARAM, "invalid bind request");
        return;
    }

    {
        std::lock_guard<std::mutex> lk(player_mtx_);
        pending_binds_[conn->ID()] = PendingBind{player_id, pkt.header.seq_id, std::chrono::steady_clock::now()};
    }

    if (logger_) logger_->Info("PlayerBind pending: player=" + std::to_string(player_id) +
                               " conn=" + std::to_string(conn->ID()) +
                               " seq=" + std::to_string(pkt.header.seq_id));

    // 转发给 Biz（附加 conn_id 用于响应路由）
    auto pool = RouteToPool(pkt.header.cmd_id);
    if (!pool) {
        if (logger_) logger_->Warn("No upstream pool for PlayerBind");
        {
            std::lock_guard<std::mutex> lk(player_mtx_);
            auto it = pending_binds_.find(conn->ID());
            if (it != pending_binds_.end() && it->second.seq_id == pkt.header.seq_id) {
                pending_binds_.erase(it);
            }
        }
        SendErrorAndClose(conn->ID(), pkt.header.seq_id, gs::errors::SERVICE_UNAVAILABLE, "bind service unavailable");
        return;
    }

    std::vector<uint8_t> extended_payload(pkt.payload.Size() + 8);
    WriteU64BE(extended_payload.data(), conn->ID());
    if (pkt.payload.Size() > 0) {
        std::memcpy(extended_payload.data() + 8, pkt.payload.Data(), pkt.payload.Size());
    }

    Packet forward_pkt = pkt;
    forward_pkt.payload = Buffer(std::move(extended_payload));
    forward_pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(forward_pkt.payload.Size());

    if (!pool->SendPacket(forward_pkt)) {
        if (logger_) logger_->Warn("Failed to forward PlayerBind to upstream");
        {
            std::lock_guard<std::mutex> lk(player_mtx_);
            pending_binds_.erase(conn->ID());
        }
    }
}

void Gateway::BindPlayer(uint64_t player_id, uint64_t conn_id) {
    uint64_t old_conn_id = 0;
    {
        std::lock_guard<std::mutex> lk(player_mtx_);
        auto current_bind = conn_player_.find(conn_id);
        if (current_bind != conn_player_.end() && current_bind->second != player_id) {
            uint64_t old_player_id = current_bind->second;
            auto old_player_it = player_conn_.find(old_player_id);
            if (old_player_it != player_conn_.end() && old_player_it->second == conn_id) {
                player_conn_.erase(old_player_it);
            }
            conn_player_.erase(current_bind);
        }

        auto old_it = player_conn_.find(player_id);
        if (old_it != player_conn_.end() && old_it->second != conn_id) {
            old_conn_id = old_it->second;
            conn_player_.erase(old_conn_id);
        }
        player_conn_[player_id] = conn_id;
        conn_player_[conn_id] = player_id;
    }

    if (logger_) logger_->Info("Player bound: player=" + std::to_string(player_id) +
                               " conn=" + std::to_string(conn_id));

    if (old_conn_id != 0) {
        if (logger_) logger_->Info("Player kick old connection: player=" + std::to_string(player_id) +
                                   " old_conn=" + std::to_string(old_conn_id) +
                                   " new_conn=" + std::to_string(conn_id));
        KickConnection(old_conn_id, "account login elsewhere");
    }
}

void Gateway::KickConnection(uint64_t conn_id, const std::string& reason) {
    // 构造踢人通知包
    protocol::PlayerKickNotify notify;
    notify.set_reason(reason);
    std::string notify_data;
    if (!notify.SerializeToString(&notify_data)) {
        if (logger_) logger_->Error("Failed to serialize kick notify for " + std::to_string(conn_id));
        return;
    }

    Packet kick_pkt;
    kick_pkt.header.magic = MAGIC_VALUE;
    kick_pkt.header.cmd_id = protocol::CMD_GW_PLAYER_KICK;
    kick_pkt.header.seq_id = 0;
    kick_pkt.header.flags = 0;
    kick_pkt.payload = Buffer::FromVector(std::vector<uint8_t>(notify_data.begin(), notify_data.end()));
    kick_pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(kick_pkt.payload.Size());

    // 加密并直接发送（不走 coalescer），写队列 drain 后再关闭
    {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        auto it = clients_.find(conn_id);
        if (it == clients_.end()) return;

        Packet out = kick_pkt;
        auto session_key = handshake_mw_->GetSessionKey(conn_id);
        if (!session_key.empty() && out.payload.Size() > 0) {
            try {
                auto encrypted = EncryptPacketPayload(session_key,
                    std::vector<uint8_t>(out.payload.Data(), out.payload.Data() + out.payload.Size()));
                out.payload = Buffer::FromVector(std::move(encrypted));
                out.header.length = HEADER_SIZE + static_cast<uint32_t>(out.payload.Size());
                out.header.flags |= static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
            } catch (const std::exception& e) {
                if (logger_) logger_->Error("Encryption failed for kick notify " + std::to_string(conn_id) + ": " + std::string(e.what()));
                return;
            }
        }

        auto ws_it = ws_clients_.find(conn_id);
        auto encoded = EncodePacket(out);
        if (ws_it != ws_clients_.end()) {
            ws_it->second->Send(encoded);
        } else {
            it->second->Send(encoded);
        }

        it->second->CloseAfterWrite();
    }
}

void Gateway::CleanupPlayerState(uint64_t conn_id) {
    std::lock_guard<std::mutex> lk(player_mtx_);
    pending_binds_.erase(conn_id);

    auto it = conn_player_.find(conn_id);
    if (it != conn_player_.end()) {
        uint64_t player_id = it->second;
        auto pit = player_conn_.find(player_id);
        if (pit != player_conn_.end() && pit->second == conn_id) {
            player_conn_.erase(pit);
            if (logger_) logger_->Info("Player unbound: player=" + std::to_string(player_id) +
                                       " conn=" + std::to_string(conn_id));
        }
        conn_player_.erase(it);
    }
}

bool Gateway::IsPlayerBound(uint64_t conn_id) {
    std::lock_guard<std::mutex> lk(player_mtx_);
    return conn_player_.find(conn_id) != conn_player_.end();
}

void Gateway::SendToClientDirect(uint64_t conn_id, const Packet& pkt) {
    SendToClientDirect(conn_id, pkt, false);
}

void Gateway::SendToClientDirect(uint64_t conn_id, const Packet& pkt, bool close_after_write) {
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    auto it = clients_.find(conn_id);
    if (it == clients_.end()) return;

    Packet out = pkt;
    auto session_key = handshake_mw_->GetSessionKey(conn_id);
    if (!session_key.empty() && out.payload.Size() > 0) {
        try {
            auto encrypted = EncryptPacketPayload(session_key,
                std::vector<uint8_t>(out.payload.Data(), out.payload.Data() + out.payload.Size()));
            out.payload = Buffer::FromVector(std::move(encrypted));
            out.header.length = HEADER_SIZE + static_cast<uint32_t>(out.payload.Size());
            out.header.flags |= static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Encryption failed for kick notify " + std::to_string(conn_id) + ": " + e.what());
            return;
        }
    }

    auto ws_it = ws_clients_.find(conn_id);
    if (ws_it != ws_clients_.end()) {
        if (close_after_write) {
            ws_it->second->Send(EncodePacket(out));
        } else {
            coalescer_->Enqueue(std::static_pointer_cast<IConnection>(ws_it->second), out);
        }
    } else {
        if (close_after_write) {
            it->second->Send(EncodePacket(out));
        } else {
            coalescer_->Enqueue(it->second, out);
        }
    }

    if (close_after_write) {
        it->second->CloseAfterWrite();
    }
}

void Gateway::SendErrorAndClose(uint64_t conn_id, uint32_t seq_id, uint32_t code, const std::string& msg) {
    Packet pkt;
    pkt.header.magic = MAGIC_VALUE;
    pkt.header.cmd_id = protocol::CMD_SYS_ERROR_PACKET;
    pkt.header.seq_id = seq_id;
    pkt.header.flags = static_cast<uint32_t>(gs::net::Flag::RPC_RES);
    pkt.payload = Buffer::FromVector(EncodeCommonResultPayload(code, msg));
    pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size());

    std::lock_guard<std::mutex> lk(sessions_mtx_);
    auto it = clients_.find(conn_id);
    if (it == clients_.end()) return;

    Packet out = pkt;
    auto session_key = handshake_mw_->GetSessionKey(conn_id);
    if (!session_key.empty() && out.payload.Size() > 0) {
        try {
            auto encrypted = EncryptPacketPayload(session_key,
                std::vector<uint8_t>(out.payload.Data(), out.payload.Data() + out.payload.Size()));
            out.payload = Buffer::FromVector(std::move(encrypted));
            out.header.length = HEADER_SIZE + static_cast<uint32_t>(out.payload.Size());
            out.header.flags |= static_cast<uint32_t>(gs::net::Flag::ENCRYPT);
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Encryption failed for error response " + std::to_string(conn_id) + ": " + e.what());
            it->second->Close();
            return;
        }
    }

    auto ws_it = ws_clients_.find(conn_id);
    auto encoded = EncodePacket(out);
    if (ws_it != ws_clients_.end()) {
        ws_it->second->Send(encoded);
    } else {
        it->second->Send(encoded);
    }
    it->second->CloseAfterWrite();
}

// ==================== main ====================

int main(int argc, char* argv[]) {
    std::string config_file = "conf/gateway.json";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        }
    }
    
    gs::gateway::Config cfg;
    try {
        cfg = gs::gateway::LoadConfig(config_file);
        std::cout << "[Config] Loaded from " << config_file << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Failed to load " << config_file << ": " << e.what() << std::endl;
        return 1;
    }
    
    gs::gateway::PrintConfig(cfg);
    
    auto logger = std::make_shared<gs::logger::Logger>(cfg.service_type, cfg.node_id);
    logger->SetLevel(gs::logger::ParseLogLevel(cfg.log_level));
    if (!cfg.log_file.empty()) {
        logger->SetOutputFile(cfg.log_file);
    }

    Gateway gw;
    gw.SetLogger(logger);
    if (!gw.Start(cfg)) {
        return 1;
    }
    
    // 注册 OS 信号处理
    signal(SIGINT, [](int) { /* 由主循环通过其他方式处理，此处仅占位 */ });
    signal(SIGTERM, [](int) { /* 同上 */ });
    
    // 简单的信号监听：通过独立线程捕获 Ctrl+C
    std::thread sig_thread([&gw]() {
#ifdef _WIN32
        // Windows 下简化处理：不做额外信号捕获，依赖控制台关闭
#else
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &set, nullptr);
        int sig;
        sigwait(&set, &sig);
        gw.Stop();
#endif
    });
    sig_thread.detach();
    
    gw.Wait();
    gw.Stop();
    return 0;
}
