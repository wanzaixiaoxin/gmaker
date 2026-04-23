#include <iostream>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "gateway_config.hpp"
#include "gs/net/async/tcp_server.hpp"
#include "gs/net/async/upstream.hpp"
#include "gs/net/async/coalescer.hpp"
#include "gs/net/packet.hpp"
#include "gs/net/address.hpp"
#include "gs/crypto/session.hpp"
#include "gs/replay/replay.hpp"
#include "gs/registry/client.hpp"
#include "gs/metrics/metrics.hpp"
#include "gs/errors.hpp"
#include "gs/logger/logger.hpp"
#include "registry.pb.h"
#include "protocol.pb.h"  // 命令 ID 定义

using namespace gs::net;
using namespace gs::net::async;
using namespace gs::crypto;

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

// Gateway 主类
class Gateway {
public:
    void SetLogger(std::shared_ptr<gs::logger::Logger> logger) { logger_ = logger; }

    bool Start(const gs::gateway::Config& cfg);
    void Stop();
    void Wait();

private:
    void OnClientConnect(AsyncTCPConnection* conn);
    void OnClientPacket(AsyncTCPConnection* conn, Packet& pkt);
    void OnClientClose(AsyncTCPConnection* conn);
    void OnUpstreamPacket(IConnection* conn, Packet& pkt);
    
    // 根据命令 ID 路由到对应的上游池
    AsyncUpstreamPool* RouteToPool(uint32_t cmd_id);
    
    std::unique_ptr<AsyncTCPServer> server_;
    std::unordered_map<std::string, std::unique_ptr<AsyncUpstreamPool>> upstream_pools_;  // 按服务类型
    std::unique_ptr<AsyncWriteCoalescer> coalescer_;
    std::unique_ptr<gs::registry::RegistryClient> reg_client_;
    
    std::mutex sessions_mtx_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> clients_;
    std::vector<uint8_t> master_key_;
    std::unique_ptr<gs::replay::Checker> replay_checker_;
    
    std::shared_ptr<gs::logger::Logger> logger_;
    std::shared_ptr<HandshakeMiddleware> handshake_mw_;
    std::shared_ptr<EncryptionMiddleware> encryption_mw_;
    
    // Room 管理
    std::mutex room_mtx_;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> room_members_;
    std::unordered_map<uint64_t, uint64_t> conn_room_;
    
    // 配置
    gs::gateway::Config cfg_;
    
    // Metrics
    gs::metrics::Counter*   req_counter_ = nullptr;
    gs::metrics::Histogram* req_latency_ = nullptr;
    gs::metrics::Gauge*     conn_gauge_  = nullptr;
    
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

    auto session_key = Session::DeriveKey(master_key_, client_random);
    
    {
        std::lock_guard<std::mutex> lk(mtx_);
        sessions_[conn->ID()] = session_key;
    }

    // 发送握手响应
    Packet res;
    res.header.magic = MAGIC_VALUE;
    res.header.cmd_id = cmd_handshake_ + 1;  // 响应命令 = 请求命令 + 1
    res.header.seq_id = pkt.header.seq_id;
    res.header.flags = 0;
    
    std::vector<uint8_t> server_random(16);
    std::random_device rd;
    for (auto& b : server_random) b = static_cast<uint8_t>(rd());
    
    res.payload = Buffer(server_random);
    res.header.length = HEADER_SIZE + static_cast<uint32_t>(res.payload.Size());
    
    conn->Send(res);
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
        auto decrypted = Session::Decrypt(session_key, 
            std::vector<uint8_t>(pkt.payload.Data(), pkt.payload.Data() + pkt.payload.Size()));
        pkt.payload = Buffer(std::move(decrypted));
    } catch (const std::exception& e) {
        if (logger_) logger_->Error("Decryption failed for client " + std::to_string(conn->ID()) + ": " + e.what());
        conn->Close();
        return false;
    }

    return true;
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
    handshake_mw_ = std::make_shared<HandshakeMiddleware>(master_key_, replay_checker_.get(), logger_, protocol::CMD_HANDSHAKE_REQ);
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
    if (logger_) logger_->Info("Gateway server started on port " + std::to_string(cfg.listen_port));

    // 初始化写聚合器
    coalescer_ = std::make_unique<AsyncWriteCoalescer>(server_->EventLoop(), cfg.coalescer_interval_ms);
    if (!coalescer_->Start()) {
        if (logger_) logger_->Error("Failed to start write coalescer");
        return false;
    }

    // 初始化上游连接池 (按服务类型)
    for (const auto& svc_type : cfg.upstream_services) {
        upstream_pools_[svc_type] = std::make_unique<AsyncUpstreamPool>(
            server_->EventLoop(),
            [this](IConnection* c, Packet& p) { OnUpstreamPacket(c, p); }
        );
    }

    // 连接 Registry 并发现服务
    if (!cfg.registry_nodes.empty()) {
        std::vector<std::pair<std::string, uint16_t>> reg_addrs;
        for (const auto& addr : cfg.registry_nodes) {
            auto pos = addr.find(':');
            if (pos != std::string::npos) {
                reg_addrs.emplace_back(addr.substr(0, pos), static_cast<uint16_t>(std::stoi(addr.substr(pos + 1))));
            }
        }
        
        reg_client_ = std::make_unique<gs::registry::RegistryClient>(nullptr, reg_addrs);
        if (reg_client_->Connect()) {
            if (logger_) logger_->Info("Connected to registry");
            
            // 发现并监听所有配置的服务
            for (const auto& svc_type : cfg.upstream_services) {
                // 初始发现
                ::registry::NodeList list;
                if (reg_client_->Discover(svc_type, &list)) {
                    auto pool = upstream_pools_[svc_type].get();
                    for (int i = 0; i < list.nodes_size(); ++i) {
                        const auto& node = list.nodes(i);
                        pool->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry discovered " + svc_type + " node: " + node.host() + ":" + std::to_string(node.port()));
                    }
                }
                
                // 监听变更
                std::string type = svc_type;
                reg_client_->Watch(svc_type, [this, type](const ::registry::NodeEvent& ev) {
                    if (!ev.has_node()) return;
                    const auto& node = ev.node();
                    auto pool = upstream_pools_[type].get();
                    if (!pool) return;
                    
                    std::string addr = node.host() + ":" + std::to_string(node.port());
                    if (ev.type() == ::registry::NodeEvent::JOIN || ev.type() == ::registry::NodeEvent::UPDATE) {
                        pool->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry event: " + type + " node JOIN/UPDATE " + addr);
                    } else if (ev.type() == ::registry::NodeEvent::LEAVE) {
                        pool->RemoveNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry event: " + type + " node LEAVE " + addr);
                    }
                });
            }
        }
    }

    // 启动所有上游池
    for (auto& [svc_type, pool] : upstream_pools_) {
        if (!pool->Start()) {
            if (logger_) logger_->Warn("Failed to start upstream pool: " + svc_type);
        } else if (logger_) {
            logger_->Info(svc_type + " upstream pool started, " + std::to_string(pool->HealthyCount()) + "/" + std::to_string(pool->TotalCount()) + " nodes");
        }
    }

    return true;
}

void Gateway::Stop() {
    if (server_) server_->Stop();
    if (coalescer_) coalescer_->Stop();
    for (auto& [_, pool] : upstream_pools_) {
        if (pool) pool->Stop();
    }
    if (reg_client_) reg_client_->Close();
    
    std::lock_guard<std::mutex> lk(stop_mtx_);
    stop_flag_ = true;
    stop_cv_.notify_all();
}

void Gateway::Wait() {
    std::unique_lock<std::mutex> lk(stop_mtx_);
    stop_cv_.wait(lk, [this] { return stop_flag_; });
}

void Gateway::OnClientConnect(AsyncTCPConnection* conn) {
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    clients_[conn->ID()] = conn;
    conn_gauge_->Inc();
    if (logger_) logger_->Info("Client connected: " + std::to_string(conn->ID()));
}

void Gateway::OnClientPacket(AsyncTCPConnection* conn, Packet& pkt) {
    auto start = std::chrono::steady_clock::now();
    req_counter_->Inc();

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

    if (!pool->SendPacket(&pkt)) {
        if (logger_) logger_->Warn("Failed to forward packet to upstream");
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    req_latency_->Observe(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()));
}

void Gateway::OnClientClose(AsyncTCPConnection* conn) {
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
    
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    clients_.erase(conn->ID());
    handshake_mw_->RemoveSession(conn->ID());
    conn_gauge_->Dec();
    if (logger_) logger_->Info("Client disconnected: " + std::to_string(conn->ID()));
}

void Gateway::OnUpstreamPacket(IConnection* conn, Packet& pkt) {
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

    // 普通响应：广播或单播
    if (pkt.payload.Size() < 8) return;
    
    uint64_t conn_id = ReadU64BE(pkt.payload.Data());
    
    // 检查是否为房间广播
    std::vector<uint64_t> targets;
    {
        std::lock_guard<std::mutex> lk(room_mtx_);
        auto it = conn_room_.find(conn_id);
        if (it != conn_room_.end()) {
            auto rit = room_members_.find(it->second);
            if (rit != room_members_.end()) {
                targets.assign(rit->second.begin(), rit->second.end());
            }
        }
    }

    // 准备响应包
    Packet res;
    res.header = pkt.header;
    res.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.Size() - 8);
    if (pkt.payload.Size() > 8) {
        res.payload = Buffer(pkt.payload.Data() + 8, pkt.payload.Size() - 8);
    }

    // 加密
    auto session_key = handshake_mw_->GetSessionKey(conn_id);
    if (!session_key.empty() && res.payload.Size() > 0) {
        try {
            auto encrypted = Session::Encrypt(session_key,
                std::vector<uint8_t>(res.payload.Data(), res.payload.Data() + res.payload.Size()));
            res.payload = Buffer(std::move(encrypted));
            res.header.length = HEADER_SIZE + static_cast<uint32_t>(res.payload.Size());
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Encryption failed: " + std::string(e.what()));
            return;
        }
    }

    // 发送
    std::lock_guard<std::mutex> lk(sessions_mtx_);
    if (targets.empty()) {
        auto it = clients_.find(conn_id);
        if (it != clients_.end()) {
            coalescer_->Enqueue(it->second, res);
        }
    } else {
        for (auto cid : targets) {
            auto it = clients_.find(cid);
            if (it != clients_.end()) {
                coalescer_->Enqueue(it->second, res);
            }
        }
    }
}

AsyncUpstreamPool* Gateway::RouteToPool(uint32_t cmd_id) {
    // 根据命令 ID 查找服务类型
    std::string svc_type;
    
    // 使用 proto 中定义的命令范围
    if (cmd_id >= protocol::CmdBiz_CMD_BIZ_BASE && cmd_id <= 0x0001FFFF) {
        svc_type = "biz";
    } else if (cmd_id >= protocol::CmdLogin_CMD_LOGIN_REQ && cmd_id <= 0x0002FFFF) {
        svc_type = "login";
    } else if (cmd_id >= protocol::CmdChat_CMD_CHAT_CREATE_ROOM_REQ && cmd_id <= 0x0003FFFF) {
        svc_type = "chat";
    } else if (cmd_id >= protocol::CmdLogStats_CMD_LOGSTATS_BASE && cmd_id <= 0x0004FFFF) {
        svc_type = "logstats";
    } else if (cmd_id >= protocol::CmdRealtime_CMD_REALTIME_BASE && cmd_id <= 0x0005FFFF) {
        svc_type = "realtime";
    }
    
    // 检查服务是否在配置的上游服务列表中
    if (!svc_type.empty()) {
        auto it = upstream_pools_.find(svc_type);
        if (it != upstream_pools_.end()) {
            return it->second.get();
        }
    }
    
    // 默认路由到第一个服务
    if (!cfg_.upstream_services.empty()) {
        auto it = upstream_pools_.find(cfg_.upstream_services[0]);
        if (it != upstream_pools_.end()) {
            return it->second.get();
        }
    }
    
    return nullptr;
}

inline std::string ToHex(uint32_t v) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << v;
    return ss.str();
}

// ==================== main ====================

int main(int argc, char* argv[]) {
    std::string config_file = "gateway.json";
    
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
    gw.Wait();
    gw.Stop();
    return 0;
}
