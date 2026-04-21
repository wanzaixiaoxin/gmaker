#include <iostream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "gs/net/async/tcp_server.hpp"
#include "gs/net/async/upstream.hpp"
#include "gs/net/packet.hpp"
#include "gs/registry/client.hpp"
#include "registry.pb.h"
#include "gs/realtime/compute_thread.hpp"
#include "gs/realtime/message.hpp"
#include "gs/metrics/metrics.hpp"

using namespace gs::net;
using namespace gs::net::async;
using namespace gs::realtime;

constexpr uint32_t CMD_REALTIME_ENTER  = 0x00020001;
constexpr uint32_t CMD_REALTIME_LEAVE  = 0x00020002;
constexpr uint32_t CMD_REALTIME_MOVE   = 0x00020003;
constexpr uint32_t CMD_REALTIME_ACTION = 0x00020004;
constexpr uint32_t CMD_REALTIME_SYNC   = 0x00020005;

// RealtimeServer 实时服
struct RealtimeServer {
    bool Start(uint16_t listen_port,
               const std::vector<std::pair<std::string, uint16_t>>& registry_addrs,
               const std::vector<std::pair<std::string, uint16_t>>& fallback_gateway_addrs) {

        // 启动 metrics
        gs::metrics::ServeDefaultHTTP(":9090");

        // 初始化 Compute Thread
        compute_ = std::make_unique<ComputeThread>();
        compute_->SetOutputCallback([this](uint32_t room_id, const RoomSnapshot& snap, const std::vector<uint64_t>& conns) {
            (void)room_id;
            OnRoomBroadcast(snap, conns);
        });

        // 创建示例房间（MVP：预创建几个房间）
        for (uint32_t i = 1; i <= 5; ++i) {
            RoomConfig cfg;
            cfg.room_id = i;
            cfg.max_players = 20;
            cfg.map_size_x = 1000.0f;
            cfg.map_size_z = 1000.0f;
            cfg.tick_rate_hz = 60;
            cfg.enable_aoi = true;
            cfg.aoi_radius = 200.0f;
            compute_->CreateRoom(cfg);
            std::cout << "Created room " << i << std::endl;
        }
        compute_->Start();

        // 启动监听（接收 Gateway 转发的客户端消息）
        AsyncTCPServer::Config cfg;
        cfg.port = listen_port;
        server_ = std::make_unique<AsyncTCPServer>(cfg);

        // 连接 Gateway（多节点，从 Registry 发现或 fallback），共享服务器事件循环
        gateway_pool_ = std::make_unique<AsyncUpstreamPool>(
            server_->EventLoop(),
            [this](IConnection* c, Packet& p) { OnGatewayPacket(c, p); }
        );

        if (!registry_addrs.empty()) {
            reg_client_ = std::make_unique<gs::registry::RegistryClient>(nullptr, registry_addrs);
            if (reg_client_->Connect()) {
                ::registry::NodeList list;
                if (reg_client_->Discover("gateway", &list)) {
                    for (int i = 0; i < list.nodes_size(); ++i) {
                        const auto& node = list.nodes(i);
                        gateway_pool_->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                    }
                }
                reg_client_->Watch("gateway", [this](const ::registry::NodeEvent& ev) {
                    if (!ev.has_node()) return;
                    const auto& node = ev.node();
                    if (ev.type() == ::registry::NodeEvent::JOIN || ev.type() == ::registry::NodeEvent::UPDATE) {
                        gateway_pool_->AddNode(node.host(), static_cast<uint16_t>(node.port()));
                    } else if (ev.type() == ::registry::NodeEvent::LEAVE) {
                        gateway_pool_->RemoveNode(node.host(), static_cast<uint16_t>(node.port()));
                    }
                });
            }
        }

        if (gateway_pool_->TotalCount() == 0) {
            for (const auto& [host, port] : fallback_gateway_addrs) {
                gateway_pool_->AddNode(host, port);
            }
        }
        if (!gateway_pool_->Start()) {
            std::cerr << "Failed to start gateway upstream pool" << std::endl;
            return false;
        }

        server_->SetCallbacks(
            [this](AsyncTCPConnection* c) { OnClientConnect(c); },
            [this](AsyncTCPConnection* c, Packet& p) { OnClientPacket(c, p); },
            [this](AsyncTCPConnection* c) { OnClientClose(c); }
        );
        if (!server_->Start()) {
            std::cerr << "Failed to start realtime server" << std::endl;
            return false;
        }
        std::cout << "Realtime server listening on port " << listen_port << std::endl;
        return true;
    }

    void Stop() {
        if (server_) server_->Stop();
        if (compute_) compute_->Stop();
        if (gateway_pool_) gateway_pool_->Stop();
        if (reg_client_) reg_client_->Close();
    }

    void Wait() {
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
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_[conn->ID()] = conn;
        std::cout << "Gateway connected: " << conn->ID() << std::endl;
    }

    void OnClientPacket(AsyncTCPConnection* conn, Packet& pkt) {
        (void)conn;
        // Gateway -> Realtime：解析客户端消息，投递到 Compute Thread
        switch (pkt.header.cmd_id) {
            case CMD_REALTIME_ENTER: {
                // payload: [room_id: 4 BE][player_id: 8 BE][spawn_x: 4 BE][spawn_z: 4 BE]
                if (pkt.payload.size() < 20) return;
                uint32_t room_id = ReadU32BE(pkt.payload.data());
                uint64_t player_id = ReadU64BE(pkt.payload.data() + 4);
                float spawn_x = *reinterpret_cast<const float*>(pkt.payload.data() + 12);
                float spawn_z = *reinterpret_cast<const float*>(pkt.payload.data() + 16);
                auto msg = std::make_unique<PlayerEnterMsg>();
                msg->player_id = player_id;
                msg->spawn_pos = {spawn_x, 0, spawn_z};
                msg->conn_id = pkt.header.seq_id; // 复用 seq_id 作为 conn_id（简化）
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_LEAVE: {
                if (pkt.payload.size() < 12) return;
                uint32_t room_id = ReadU32BE(pkt.payload.data());
                uint64_t player_id = ReadU64BE(pkt.payload.data() + 4);
                auto msg = std::make_unique<PlayerLeaveMsg>();
                msg->player_id = player_id;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_MOVE: {
                if (pkt.payload.size() < 24) return;
                uint32_t room_id = ReadU32BE(pkt.payload.data());
                uint64_t player_id = ReadU64BE(pkt.payload.data() + 4);
                float x = *reinterpret_cast<const float*>(pkt.payload.data() + 12);
                float z = *reinterpret_cast<const float*>(pkt.payload.data() + 16);
                float yaw = *reinterpret_cast<const float*>(pkt.payload.data() + 20);
                auto msg = std::make_unique<PlayerMoveMsg>();
                msg->player_id = player_id;
                msg->target_pos = {x, 0, z};
                msg->target_yaw = yaw;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_ACTION: {
                if (pkt.payload.size() < 20) return;
                uint32_t room_id = ReadU32BE(pkt.payload.data());
                uint64_t player_id = ReadU64BE(pkt.payload.data() + 4);
                uint32_t action_id = ReadU32BE(pkt.payload.data() + 12);
                float x = *reinterpret_cast<const float*>(pkt.payload.data() + 16);
                float z = *reinterpret_cast<const float*>(pkt.payload.data() + 20);
                auto msg = std::make_unique<PlayerActionMsg>();
                msg->player_id = player_id;
                msg->action_id = action_id;
                msg->target_pos = {x, 0, z};
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            default:
                break;
        }
    }

    void OnClientClose(AsyncTCPConnection* conn) {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_.erase(conn->ID());
    }

    void OnGatewayPacket(IConnection* conn, Packet& pkt) {
        (void)conn;
        (void)pkt;
        // Gateway -> Realtime 的响应（当前 Realtime 主动发消息给 Gateway，不需要处理响应）
    }

    // Room 广播：将 Snapshot 编码为 Packet，发送到 Gateway
    void OnRoomBroadcast(const RoomSnapshot& snap, const std::vector<uint64_t>& target_conns) {
        // 编码 RoomSnapshot 为二进制 payload
        // [frame_seq: 4 BE][timestamp: 8 BE][player_count: 4 BE]
        // 每个玩家: [player_id: 8 BE][x: 4][y: 4][z: 4][yaw: 4][hp: 4][anim: 4]
        std::vector<uint8_t> payload;
        payload.reserve(16 + snap.players.size() * 32);
        auto append_u32 = [&payload](uint32_t v) {
            payload.resize(payload.size() + 4);
            WriteU32BE(&payload[payload.size() - 4], v);
        };
        auto append_u64 = [&payload](uint64_t v) {
            payload.resize(payload.size() + 8);
            WriteU64BE(&payload[payload.size() - 8], v);
        };
        append_u32(snap.frame_seq);
        append_u64(snap.timestamp_ms);
        append_u32(static_cast<uint32_t>(snap.players.size()));
        for (const auto& p : snap.players) {
            append_u64(p.player_id);
            auto append_f = [&payload](float v) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&v);
                payload.insert(payload.end(), bytes, bytes + 4);
            };
            append_f(p.pos.x);
            append_f(p.pos.y);
            append_f(p.pos.z);
            append_f(p.yaw);
            append_u32(p.hp);
            append_u32(p.anim_state);
        }

        // 每个 target_conn 发送一个定向包
        for (uint64_t conn_id : target_conns) {
            Packet pkt;
            pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(payload.size());
            pkt.header.magic = MAGIC_VALUE;
            pkt.header.cmd_id = CMD_REALTIME_SYNC;
            pkt.header.seq_id = static_cast<uint32_t>(conn_id); // 复用 seq_id 标记目标 conn
            pkt.header.flags = static_cast<uint32_t>(Flag::BROADCAST);
            pkt.payload = payload;
            gateway_pool_->SendPacket(pkt);
        }
    }

    std::unique_ptr<AsyncTCPServer> server_;
    std::unique_ptr<AsyncUpstreamPool> gateway_pool_;
    std::unique_ptr<gs::registry::RegistryClient> reg_client_;
    std::unique_ptr<ComputeThread> compute_;

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> conns_;

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
    uint16_t port = 8084;
    std::vector<std::pair<std::string, uint16_t>> registry_addrs = {{"127.0.0.1", 2379}};
    std::vector<std::pair<std::string, uint16_t>> fallback_gw = {{"127.0.0.1", 8081}};

    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc > 2) registry_addrs = ParseAddrList(argv[2]);
    if (argc > 3) fallback_gw = ParseAddrList(argv[3]);

    RealtimeServer srv;
    if (!srv.Start(port, registry_addrs, fallback_gw)) {
        return 1;
    }
    srv.Wait();
    srv.Stop();
    return 0;
}
