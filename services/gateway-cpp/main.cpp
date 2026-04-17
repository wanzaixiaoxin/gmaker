#include <iostream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "gs/net/tcp_server.hpp"
#include "gs/net/upstream.hpp"
#include "gs/net/packet.hpp"
#include "gs/net/coalescer.hpp"
#include "gs/crypto/session.hpp"
#include "gs/replay/replay.hpp"
#include "gs/registry/client.hpp"
#include "gs/metrics/metrics.hpp"
#include "gs/errors.hpp"

using namespace gs::net;
using namespace gs::crypto;

constexpr uint32_t CMD_HANDSHAKE = 0x00000002;

// BizNodeConfig 描述一个 Biz 后端节点
struct BizNodeConfig {
    std::string host;
    uint16_t    port;
};

class Gateway {
public:
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

        // 初始化写聚合器（Write Coalescing）
        coalescer_ = std::make_unique<WriteCoalescer>(16); // 16ms 聚合窗口
        coalescer_->Start();

        // 初始化 UpstreamPool
        biz_pool_ = std::make_unique<UpstreamPool>(
            [this](TCPConn* c, Packet& p) { OnBizPacket(c, p); }
        );

        // 连接 Registry（多节点）
        if (!registry_addrs.empty()) {
            reg_client_ = std::make_unique<gs::registry::RegistryClient>(registry_addrs);
            if (!reg_client_->Connect()) {
                std::cerr << "Failed to connect to registry, using fallback biz nodes" << std::endl;
            } else {
                std::cout << "Connected to registry, discovering biz nodes..." << std::endl;

                // 初始发现
                ::registry::NodeList list;
                if (reg_client_->Discover("biz", &list)) {
                    for (int i = 0; i < list.nodes_size(); ++i) {
                        const auto& node = list.nodes(i);
                        biz_pool_->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                        std::cout << "Registry discovered biz node: " << node.host()
                                  << ":" << node.port() << std::endl;
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
                        std::cout << "Registry event: biz node JOIN/UPDATE " << addr << std::endl;
                    } else if (ev.type() == ::registry::NodeEvent::LEAVE) {
                        biz_pool_->RemoveNode(node.host(), static_cast<uint16_t>(node.port()));
                        std::cout << "Registry event: biz node LEAVE " << addr << std::endl;
                    }
                });
            }
        }

        // 如果没有从 Registry 发现任何节点，使用 fallback
        if (biz_pool_->TotalCount() == 0) {
            if (fallback_biz_nodes.empty()) {
                std::cerr << "No biz nodes configured and no registry discovery" << std::endl;
                return false;
            }
            for (const auto& node : fallback_biz_nodes) {
                biz_pool_->AddNode(node.host, node.port);
            }
            std::cout << "Using fallback biz nodes" << std::endl;
        }

        if (!biz_pool_->Start()) {
            std::cerr << "Failed to start biz upstream pool" << std::endl;
            return false;
        }
        std::cout << "Biz upstream pool started, " << biz_pool_->HealthyCount()
                  << "/" << biz_pool_->TotalCount() << " nodes connected" << std::endl;

        // 启动 Gateway 监听
        TCPServer::Config cfg;
        cfg.port = listen_port;
        server_ = std::make_unique<TCPServer>(cfg);
        server_->SetCallbacks(
            [this](TCPConn* c) { OnClientConnect(c); },
            [this](TCPConn* c, Packet& p) { OnClientPacket(c, p); },
            [this](TCPConn* c) { OnClientClose(c); }
        );
        if (!server_->Start()) {
            std::cerr << "Failed to start gateway server" << std::endl;
            return false;
        }
        std::cout << "Gateway listening on port " << listen_port << std::endl;
        return true;
    }

    void Stop() {
        if (server_) server_->Stop();
        if (coalescer_) coalescer_->Stop();
        if (biz_pool_) biz_pool_->Stop();
        if (reg_client_) reg_client_->Close();
    }

    void Wait() {
        std::cout << "Gateway running, waiting for signal..." << std::endl;
        std::unique_lock<std::mutex> lk(stop_mtx_);
        stop_cv_.wait(lk, [this] { return stop_flag_; });
    }

    void SignalStop() {
        std::lock_guard<std::mutex> lk(stop_mtx_);
        stop_flag_ = true;
        stop_cv_.notify_all();
    }

private:
    void OnClientConnect(TCPConn* conn) {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        clients_[conn->ID()] = conn;
        conn_gauge_->Inc();
        std::cout << "Client connected: " << conn->ID() << std::endl;
    }

    void OnClientPacket(TCPConn* conn, Packet& pkt) {
        auto start = std::chrono::steady_clock::now();
        req_counter_->Inc();

        if (pkt.header.cmd_id == CMD_HANDSHAKE) {
            HandleHandshake(conn, pkt);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            req_latency_->Observe(static_cast<double>(ms));
            return;
        }

        // 检查是否已完成握手（强制加密模式）
        bool encrypted = false;
        {
            std::lock_guard<std::mutex> lk(sessions_mtx_);
            encrypted = sessions_.find(conn->ID()) != sessions_.end();
        }
        if (!encrypted) {
            std::cerr << "Client " << conn->ID() << " sent packet before handshake, closing" << std::endl;
            conn->Close();
            return;
        }

        // 解密 payload
        if (HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
            std::vector<uint8_t> key;
            {
                std::lock_guard<std::mutex> lk(sessions_mtx_);
                key = sessions_[conn->ID()];
            }
            try {
                pkt.payload = DecryptPacketPayload(key, pkt.payload);
                pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
            } catch (const std::exception& e) {
                std::cerr << "Decrypt failed for client " << conn->ID() << ": " << e.what() << std::endl;
                conn->Close();
                return;
            }
        }

        // 转发到 Biz（轮询负载均衡）
        pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.size());
        if (!biz_pool_->SendPacket(pkt)) {
            std::cerr << "No healthy biz node available to forward packet" << std::endl;
            // TODO: 向客户端返回 SERVICE_UNAVAILABLE 错误包
        }

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        req_latency_->Observe(static_cast<double>(ms));
    }

    void OnBizPacket(TCPConn* conn, Packet& pkt) {
        (void)conn;
        // 使用 WriteCoalescer 聚合广播：收集所有目标连接后批量发送
        std::vector<TCPConn*> targets_plain;
        std::vector<uint64_t> targets_encrypted_ids;
        std::vector<TCPConn*> targets_encrypted_conns;
        {
            std::lock_guard<std::mutex> lk(sessions_mtx_);
            for (auto& [id, client] : clients_) {
                auto it = sessions_.find(id);
                if (it != sessions_.end()) {
                    targets_encrypted_ids.push_back(id);
                    targets_encrypted_conns.push_back(client);
                } else {
                    targets_plain.push_back(client);
                }
            }
        }

        // 明文连接直接通过 coalescer 广播
        if (!targets_plain.empty()) {
            coalescer_->Broadcast(targets_plain, pkt);
        }

        // 加密连接：各自加密后通过 coalescer 发送
        if (!targets_encrypted_conns.empty()) {
            std::lock_guard<std::mutex> lk(sessions_mtx_);
            for (size_t i = 0; i < targets_encrypted_conns.size(); ++i) {
                uint64_t id = targets_encrypted_ids[i];
                TCPConn* client = targets_encrypted_conns[i];
                auto it = sessions_.find(id);
                if (it == sessions_.end()) continue;
                try {
                    Packet encrypted_pkt = pkt;
                    encrypted_pkt.payload = EncryptPacketPayload(it->second, pkt.payload);
                    encrypted_pkt.header.flags |= static_cast<uint32_t>(Flag::ENCRYPT);
                    encrypted_pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(encrypted_pkt.payload.size());
                    coalescer_->Enqueue(client, encrypted_pkt);
                } catch (const std::exception& e) {
                    std::cerr << "Encrypt failed for client " << id << ": " << e.what() << std::endl;
                }
            }
        }
    }

    void OnClientClose(TCPConn* conn) {
        std::lock_guard<std::mutex> lk(sessions_mtx_);
        clients_.erase(conn->ID());
        sessions_.erase(conn->ID());
        conn_gauge_->Dec();
        std::cout << "Client disconnected: " << conn->ID() << std::endl;
    }

    void HandleHandshake(TCPConn* conn, Packet& pkt) {
        // HandshakeReq v1: [version: 1][timestamp: 8 BE][nonce: 8][client_random: 16] = 33 bytes
        constexpr size_t kHandshakeV1Size = 1 + 8 + 8 + 16;
        if (pkt.payload.size() < kHandshakeV1Size) {
            std::cerr << "Invalid handshake request from client " << conn->ID() << std::endl;
            conn->Close();
            return;
        }

        uint8_t version = pkt.payload[0];
        if (version != 1) {
            std::cerr << "Unsupported handshake version: " << (int)version << std::endl;
            conn->Close();
            return;
        }

        uint64_t timestamp = ReadU64BE(pkt.payload.data() + 1);
        std::string nonce(pkt.payload.begin() + 9, pkt.payload.begin() + 17);
        std::vector<uint8_t> client_random(pkt.payload.begin() + 17, pkt.payload.begin() + 33);

        // 防重放检查
        auto ts = std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
        if (!replay_checker_.Check(ts, nonce)) {
            std::cerr << "Replay detected or invalid timestamp from client " << conn->ID() << std::endl;
            conn->Close();
            return;
        }

        std::vector<uint8_t> server_random = RandomBytes(16);

        std::vector<uint8_t> session_key;
        try {
            session_key = DeriveSessionKey(master_key_, client_random, server_random);
        } catch (const std::exception& e) {
            std::cerr << "Session key derivation failed: " << e.what() << std::endl;
            conn->Close();
            return;
        }

        std::vector<uint8_t> encrypted_challenge;
        try {
            encrypted_challenge = EncryptPacketPayload(session_key, client_random);
        } catch (const std::exception& e) {
            std::cerr << "Handshake encrypt failed: " << e.what() << std::endl;
            conn->Close();
            return;
        }

        // HandshakeRes v1: [version: 1][server_random: 16][encrypted_challenge: variable]
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
            std::lock_guard<std::mutex> lk(sessions_mtx_);
            sessions_[conn->ID()] = std::move(session_key);
        }
        std::cout << "Handshake completed with client " << conn->ID() << std::endl;
    }

    std::unique_ptr<TCPServer> server_;
    std::unique_ptr<UpstreamPool> biz_pool_;
    std::unique_ptr<WriteCoalescer> coalescer_;
    std::unique_ptr<gs::registry::RegistryClient> reg_client_;
    std::mutex sessions_mtx_;
    std::unordered_map<uint64_t, TCPConn*> clients_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> sessions_;
    std::vector<uint8_t> master_key_;
    gs::replay::Checker replay_checker_{std::chrono::seconds(300)};

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
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif
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

    if (argc > 1) gateway_port = static_cast<uint16_t>(std::atoi(argv[1]));
    // 支持命令行传入 registry 地址，格式: host1:port1,host2:port2
    if (argc > 2) {
        registry_addrs = ParseAddrList(argv[2]);
    }
    // 支持命令行传入 fallback biz 节点，格式: host1:port1,host2:port2
    if (argc > 3) {
        fallback_biz_nodes.clear();
        auto addrs = ParseAddrList(argv[3]);
        for (const auto& [host, port] : addrs) {
            fallback_biz_nodes.push_back({host, port});
        }
    }

    std::vector<uint8_t> master_key(32, 0);

    Gateway gw;
    if (!gw.Start(gateway_port, registry_addrs, fallback_biz_nodes, master_key)) {
        return 1;
    }
    gw.Wait();
    gw.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
