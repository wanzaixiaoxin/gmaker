#include <iostream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "gs/net/async/tcp_server.hpp"
#include "gs/net/async/upstream.hpp"
#include "gs/net/async/coalescer.hpp"
#include "gs/net/packet.hpp"
#include "gs/crypto/session.hpp"
#include "gs/replay/replay.hpp"
#include "gs/registry/client.hpp"
#include "gs/metrics/metrics.hpp"
#include "gs/errors.hpp"
#include "gs/logger/logger.hpp"
#include "registry.pb.h"

using namespace gs::net;
using namespace gs::net::async;
using namespace gs::crypto;

static gs::logger::Level ParseLogLevel(const std::string& s) {
    if (s == "debug") return gs::logger::Level::Debug;
    if (s == "warn")  return gs::logger::Level::Warn;
    if (s == "error") return gs::logger::Level::Error;
    if (s == "fatal") return gs::logger::Level::Fatal;
    return gs::logger::Level::Info;
}

constexpr uint32_t CMD_HANDSHAKE = 0x00000002;

// BizNodeConfig 描述一个 Biz 后端节点
struct BizNodeConfig {
    std::string host;
    uint16_t    port;
};

// HandshakeMiddleware 处理 Gateway-Client 握手
class HandshakeMiddleware : public Middleware {
public:
    HandshakeMiddleware(const std::vector<uint8_t>& master_key,
                        gs::replay::Checker* replay_checker,
                        std::shared_ptr<gs::logger::Logger> logger)
        : master_key_(master_key), replay_checker_(replay_checker), logger_(std::move(logger)) {}

    bool OnPacket(IConnection* conn, Packet& pkt) override {
        if (pkt.header.cmd_id != CMD_HANDSHAKE) {
            return true; // 非握手包，透传
        }
        HandleHandshake(conn, pkt);
        return false; // 握手包已处理，不继续传递
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
    void HandleHandshake(IConnection* conn, Packet& pkt) {
        constexpr size_t kHandshakeV1Size = 1 + 8 + 8 + 16;
        if (pkt.payload.size() < kHandshakeV1Size) {
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

        uint64_t timestamp = ReadU64BE(pkt.payload.data() + 1);
        std::string nonce(pkt.payload.begin() + 9, pkt.payload.begin() + 17);
        std::vector<uint8_t> client_random(pkt.payload.begin() + 17, pkt.payload.begin() + 33);

        auto ts = std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
        if (!replay_checker_->Check(ts, nonce)) {
            if (logger_) logger_->Error("Replay detected or invalid timestamp from client " + std::to_string(conn->ID()));
            conn->Close();
            return;
        }

        std::vector<uint8_t> server_random = RandomBytes(16);

        std::vector<uint8_t> session_key;
        try {
            session_key = DeriveSessionKey(master_key_, client_random, server_random);
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Session key derivation failed: " + std::string(e.what()));
            conn->Close();
            return;
        }

        std::vector<uint8_t> encrypted_challenge;
        try {
            encrypted_challenge = EncryptPacketPayload(session_key, client_random);
        } catch (const std::exception& e) {
            if (logger_) logger_->Error("Handshake encrypt failed: " + std::string(e.what()));
            conn->Close();
            return;
        }

        Packet res;
        res.header.cmd_id = CMD_HANDSHAKE;
        res.header.magic = MAGIC_VALUE;
        res.header.seq_id = pkt.header.seq_id;
        res.header.flags = 0;
        res.payload.reserve(1 + 16 + encrypted_challenge.size());
        res.payload.push_back(1); // version
        res.payload.insert(res.payload.end(), server_random.begin(), server_random.end());
        res.payload.insert(res.payload.end(), encrypted_challenge.begin(), encrypted_challenge.end());
        res.header.length = HEADER_SIZE + static_cast<uint32_t>(res.payload.size());
        conn->SendPacket(res);

        {
            std::lock_guard<std::mutex> lk(mtx_);
            sessions_[conn->ID()] = std::move(session_key);
        }
        if (logger_) logger_->Info("Handshake completed with client " + std::to_string(conn->ID()));
    }

    std::vector<uint8_t> master_key_;
    gs::replay::Checker* replay_checker_;
    std::shared_ptr<gs::logger::Logger> logger_;
    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> sessions_;
};

// EncryptionMiddleware 处理 Gateway-Client 加密/解密
class EncryptionMiddleware : public Middleware {
public:
    EncryptionMiddleware(HandshakeMiddleware* handshake_mw, std::shared_ptr<gs::logger::Logger> logger)
        : handshake_mw_(handshake_mw), gateway_logger_(logger) {}

    bool OnPacket(IConnection* conn, Packet& pkt) override {
        // 握手完成检查
        if (!handshake_mw_->IsHandshaked(conn->ID())) {
            if (gateway_logger_) gateway_logger_->Error("Client " + std::to_string(conn->ID()) + " sent packet before handshake, closing");
            conn->Close();
            return false;
        }

        // 解密 payload
        if (HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
            auto key = handshake_mw_->GetSessionKey(conn->ID());
            if (key.empty()) {
                if (gateway_logger_) gateway_logger_->Error("No session key for client " + std::to_string(conn->ID()));
                conn->Close();
                return false;
            }
            try {
                pkt.payload = DecryptPacketPayload(key, pkt.payload);
                pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
            } catch (const std::exception& e) {
                if (gateway_logger_) gateway_logger_->Error("Decrypt failed for client " + std::to_string(conn->ID()) + ": " + e.what());
                conn->Close();
                return false;
            }
        }
        return true;
    }

private:
    HandshakeMiddleware* handshake_mw_;
    std::shared_ptr<gs::logger::Logger> gateway_logger_;
};

class Gateway {
public:
    void SetLogger(std::shared_ptr<gs::logger::Logger> logger) { logger_ = logger; }

    bool Start(uint16_t listen_port,
               const std::vector<std::pair<std::string, uint16_t>>& registry_addrs,
               const std::vector<BizNodeConfig>& fallback_biz_nodes,
               const std::vector<uint8_t>& master_key,
               const std::string& metrics_addr = ":9081") {
        master_key_ = master_key;

        // 启动 metrics HTTP 服务
        gs::metrics::ServeDefaultHTTP(metrics_addr);
        req_counter_ = gs::metrics::DefaultCounter("gateway_requests_total");
        req_latency_ = gs::metrics::DefaultHistogram("gateway_request_duration_ms", {1, 5, 10, 25, 50, 100, 250, 500, 1000});
        conn_gauge_  = gs::metrics::DefaultGauge("gateway_connections");

        // 启动 Gateway 监听（先创建服务器，以便共享事件循环）
        AsyncTCPServer::Config cfg;
        cfg.port = listen_port;
        server_ = std::make_unique<AsyncTCPServer>(cfg);

        // 初始化写聚合器（Write Coalescing），共享服务器事件循环
        coalescer_ = std::make_unique<AsyncWriteCoalescer>(server_->EventLoop(), 16);
        coalescer_->Start();

        // 初始化 UpstreamPool，共享服务器事件循环
        biz_pool_ = std::make_unique<AsyncUpstreamPool>(
            server_->EventLoop(),
            [this](IConnection* c, Packet& p) { OnBizPacket(c, p); }
        );

        // 连接 Registry（多节点）
        if (!registry_addrs.empty()) {
            reg_client_ = std::make_unique<gs::registry::RegistryClient>(nullptr, registry_addrs);
            if (!reg_client_->Connect()) {
                if (logger_) logger_->Warn("Failed to connect to registry, using fallback biz nodes");
            } else {
                if (logger_) logger_->Info("Connected to registry, discovering biz nodes...");

                // 初始发现
                ::registry::NodeList list;
                if (reg_client_->Discover("biz", &list)) {
                    for (int i = 0; i < list.nodes_size(); ++i) {
                        const auto& node = list.nodes(i);
                        biz_pool_->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry discovered biz node: " + node.host() + ":" + std::to_string(node.port()));
                    }
                }

                // 监听动态变更
                reg_client_->Watch("biz", [this](const ::registry::NodeEvent& ev) {
                    if (!ev.has_node()) return;
                    const auto& node = ev.node();
                    std::string addr = node.host() + ":" + std::to_string(node.port());
                    if (ev.type() == ::registry::NodeEvent::JOIN ||
                        ev.type() == ::registry::NodeEvent::UPDATE) {
                        biz_pool_->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry event: biz node JOIN/UPDATE " + addr);
                    } else if (ev.type() == ::registry::NodeEvent::LEAVE) {
                        biz_pool_->RemoveNode(node.host(), static_cast<uint16_t>(node.port()));
                        if (logger_) logger_->Info("Registry event: biz node LEAVE " + addr);
                    }
                });
            }
        }

        // 如果没有从 Registry 发现任何节点，使用 fallback
        if (biz_pool_->TotalCount() == 0) {
            if (fallback_biz_nodes.empty()) {
                if (logger_) logger_->Error("No biz nodes configured and no registry discovery");
                return false;
            }
            for (const auto& node : fallback_biz_nodes) {
                biz_pool_->AddNode(node.host, node.port);
            }
            if (logger_) logger_->Info("Using fallback biz nodes");
        }

        if (!biz_pool_->Start()) {
            if (logger_) logger_->Error("Failed to start biz upstream pool");
            return false;
        }
        if (logger_) logger_->Info("Biz upstream pool started, " + std::to_string(biz_pool_->HealthyCount()) + "/" + std::to_string(biz_pool_->TotalCount()) + " nodes connected");

        // 初始化中间件
        handshake_mw_ = std::make_shared<HandshakeMiddleware>(master_key_, &replay_checker_, logger_);
        encryption_mw_ = std::make_shared<EncryptionMiddleware>(handshake_mw_.get(), logger_);

        server_->SetCallbacks(
            [this](AsyncTCPConnection* c) { OnClientConnect(c); },
            [this](AsyncTCPConnection* c, Packet& p) { OnClientPacket(c, p); },
            [this](AsyncTCPConnection* c) { OnClientClose(c); }
        );
        // 注册中间件：先握手，后加密解密
        server_->Use(handshake_mw_);
        server_->Use(encryption_mw_);

        if (!server_->Start()) {
            if (logger_) logger_->Error("Failed to start gateway server");
            return false;
        }
        if (logger_) logger_->Info("Gateway listening on port " + std::to_string(listen_port));
        return true;
    }

    void Stop() {
        if (server_) server_->Stop();
        if (coalescer_) coalescer_->Stop();
        if (biz_pool_) biz_pool_->Stop();
        if (reg_client_) reg_client_->Close();
    }

    void Wait() {
        if (logger_) logger_->Info("Gateway running, waiting for signal...");
        std::unique_lock<std::mutex> lk(stop_mtx_);
        stop_cv_.wait(lk, [this] { return stop_flag_; });
    }

    void SignalStop() {
        std::lock_guard<std::mutex> lk(stop_mtx_);
        stop_flag_ = true;
        stop_cv_.notify_all();
    }

private:
    void OnClientConnect(AsyncTCPConnection* conn) {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        clients_[conn->ID()] = conn;
        conn_gauge_->Inc();
        if (logger_) logger_->Info("Client connected: " + std::to_string(conn->ID()));
    }

    void OnClientPacket(AsyncTCPConnection* conn, Packet& pkt) {
        (void)conn;
        auto start = std::chrono::steady_clock::now();
        req_counter_->Inc();

        // 转发到 Biz（轮询负载均衡）
        pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.size());
        if (!biz_pool_->SendPacket(pkt)) {
            if (logger_) logger_->Warn("No healthy biz node available to forward packet");
            // TODO: 向客户端返回 SERVICE_UNAVAILABLE 错误包
        }

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        req_latency_->Observe(static_cast<double>(ms));
    }

    void OnBizPacket(IConnection* conn, Packet& pkt) {
        (void)conn;
        // 单次加锁完成所有目标收集和加密发送，避免并发断开导致 UAF
        std::lock_guard<std::mutex> lk(sessions_mtx_);

        std::vector<IConnection*> targets_plain;
        std::vector<std::pair<IConnection*, std::vector<uint8_t>>> targets_encrypted;

        for (auto& [id, client] : clients_) {
            if (handshake_mw_->IsHandshaked(id)) {
                targets_encrypted.emplace_back(client, handshake_mw_->GetSessionKey(id));
            } else {
                targets_plain.push_back(client);
            }
        }

        // 明文连接直接通过 coalescer 广播
        if (!targets_plain.empty()) {
            coalescer_->Broadcast(targets_plain, pkt);
        }

        // 加密连接：各自加密后通过 coalescer 发送
        for (auto& [client, key] : targets_encrypted) {
            try {
                Packet encrypted_pkt = pkt;
                encrypted_pkt.payload = EncryptPacketPayload(key, pkt.payload);
                encrypted_pkt.header.flags |= static_cast<uint32_t>(Flag::ENCRYPT);
                encrypted_pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(encrypted_pkt.payload.size());
                coalescer_->Enqueue(client, encrypted_pkt);
            } catch (const std::exception& e) {
                if (logger_) logger_->Error("Encrypt failed: " + std::string(e.what()));
            }
        }
    }

    void OnClientClose(AsyncTCPConnection* conn) {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        clients_.erase(conn->ID());
        handshake_mw_->RemoveSession(conn->ID());
        conn_gauge_->Dec();
        if (logger_) logger_->Info("Client disconnected: " + std::to_string(conn->ID()));
    }

    std::unique_ptr<AsyncTCPServer> server_;
    std::unique_ptr<AsyncUpstreamPool> biz_pool_;
    std::unique_ptr<AsyncWriteCoalescer> coalescer_;
    std::unique_ptr<gs::registry::RegistryClient> reg_client_;
    std::mutex sessions_mtx_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> clients_;
    std::vector<uint8_t> master_key_;
    gs::replay::Checker replay_checker_{std::chrono::seconds(300)};

    std::shared_ptr<gs::logger::Logger> logger_;
    std::shared_ptr<HandshakeMiddleware> handshake_mw_;
    std::shared_ptr<EncryptionMiddleware> encryption_mw_;

    // metrics
    gs::metrics::Counter*   req_counter_ = nullptr;
    gs::metrics::Histogram* req_latency_ = nullptr;
    gs::metrics::Gauge*     conn_gauge_  = nullptr;

    std::mutex stop_mtx_;
    std::condition_variable stop_cv_;
    bool stop_flag_ = false;
};

static std::vector<std::pair<std::string, uint16_t>> ParseAddrList(const std::string& arg) {
    std::vector<std::pair<std::string, uint16_t>> out;
    size_t pos = 0;
    while (pos < arg.size()) {
        size_t comma = arg.find(',', pos);
        if (comma == std::string::npos) comma = arg.size();
        std::string pair = arg.substr(pos, comma - pos);
        size_t colon = pair.find(':');
        if (colon != std::string::npos) {
            out.push_back({pair.substr(0, colon),
                static_cast<uint16_t>(std::atoi(pair.substr(colon + 1).c_str()))});
        }
        pos = comma + 1;
    }
    return out;
}

int main(int argc, char* argv[]) {
    uint16_t gateway_port = 8081;

    // Registry 地址（多节点）
    std::vector<std::pair<std::string, uint16_t>> registry_addrs = {
        {"127.0.0.1", 2379},
    };

    // Fallback Biz 节点（当 Registry 不可用时使用）
    std::vector<BizNodeConfig> fallback_biz_nodes = {
        {"127.0.0.1", 8082},
        {"127.0.0.1", 8083},
    };

    std::string log_file;
    std::string log_level = "info";

    // 简单命令行解析：位置参数在前，--log-file / --log-level 为可选命名参数
    int pos_idx = 1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log-file" && i + 1 < argc) {
            log_file = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (pos_idx == 1) {
            gateway_port = static_cast<uint16_t>(std::atoi(arg.c_str()));
            pos_idx++;
        } else if (pos_idx == 2) {
            registry_addrs = ParseAddrList(arg);
            pos_idx++;
        } else if (pos_idx == 3) {
            fallback_biz_nodes.clear();
            auto addrs = ParseAddrList(arg);
            for (const auto& [host, port] : addrs) {
                fallback_biz_nodes.push_back({host, port});
            }
            pos_idx++;
        }
    }

    std::vector<uint8_t> master_key(32, 0);

    auto logger = std::make_shared<gs::logger::Logger>("gateway", "gateway-1");
    logger->SetLevel(ParseLogLevel(log_level));
    if (!log_file.empty()) {
        logger->SetOutputFile(log_file);
    }

    Gateway gw;
    gw.SetLogger(logger);
    if (!gw.Start(gateway_port, registry_addrs, fallback_biz_nodes, master_key)) {
        return 1;
    }
    gw.Wait();
    gw.Stop();
    return 0;
}
