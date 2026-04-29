#include <iostream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "net/async/tcp_server.hpp"
#include "net/packet.hpp"
#include "net/address.hpp"
#include "discovery/factory.hpp"
#include "realtime/compute_thread.hpp"
#include "realtime/message.hpp"
#include "metrics/metrics.hpp"
#include "logger/logger.hpp"
#include "protocol.pb.h"

using namespace gs::net;
using namespace gs::net::async;
using namespace gs::realtime;

constexpr uint32_t CMD_REALTIME_ENTER  = protocol::CMD_RT_ROOM_ENTER_REQ;
constexpr uint32_t CMD_REALTIME_LEAVE  = protocol::CMD_RT_ROOM_LEAVE_REQ;
constexpr uint32_t CMD_REALTIME_MOVE   = 0x00020020; // 预留：玩家移动（proto 待补充）
constexpr uint32_t CMD_REALTIME_ACTION = 0x00020021; // 预留：玩家动作（proto 待补充）
constexpr uint32_t CMD_REALTIME_SYNC   = protocol::CMD_RT_STATE_SYNC;

// RealtimeServer 实时服
// Gateway 主动连接到 Realtime，Realtime 通过已有连接回推 Snapshot
struct RealtimeServer {
    void SetLogger(std::shared_ptr<gs::logger::Logger> logger) { logger_ = logger; }

    bool Start(uint16_t listen_port,
               const std::string& discovery_type,
               const std::vector<std::string>& discovery_addrs) {

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
            if (logger_) logger_->Info("Created room " + std::to_string(i));
        }
        compute_->Start();

        // 启动监听（接收 Gateway 转发的客户端消息）
        AsyncTCPServer::Config cfg;
        cfg.port = listen_port;
        server_ = std::make_unique<AsyncTCPServer>(cfg);
        server_->SetCallbacks(
            [this](AsyncTCPConnection* c) { OnClientConnect(c); },
            [this](AsyncTCPConnection* c, Packet& p) { OnClientPacket(c, p); },
            [this](AsyncTCPConnection* c) { OnClientClose(c); }
        );
        if (!server_->Start()) {
            if (logger_) logger_->Error("Failed to start realtime server");
            return false;
        }
        if (logger_) logger_->Info("Realtime server started on port " + std::to_string(listen_port));

        // 注册到服务发现后端，使用 discovery 封装层
        if (!discovery_addrs.empty()) {
            sd_ = gs::discovery::CreateDiscovery(discovery_type, discovery_addrs);
            if (!sd_) {
                if (logger_) logger_->Error("Failed to create discovery, exiting");
                return false;
            }
            gs::discovery::NodeInfo node;
            node.service_type = "realtime";
            node.node_id = "realtime-1";
            node.host = "127.0.0.1";
            node.port = listen_port;
            node.register_at = static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count());
            if (sd_->Register(node)) {
                if (logger_) logger_->Info("Realtime registered to " + discovery_type);
                heartbeat_thread_ = std::thread([this]() { HeartbeatLoop(); });
            } else {
                if (logger_) logger_->Warn("Realtime register failed");
            }
        }

        if (logger_) logger_->Info("Realtime server listening on port " + std::to_string(listen_port));
        return true;
    }

    void Stop() {
        heartbeat_stop_.store(true);
        if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
        if (server_) server_->Stop();
        if (compute_) compute_->Stop();
        if (sd_) sd_->Close();
    }

    void HeartbeatLoop() {
        // 心跳已由 discovery::RegistryImpl 内部自动管理，此处保留空实现
        while (!heartbeat_stop_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
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
        if (logger_) logger_->Info("Gateway connected: " + std::to_string(conn->ID()));
    }

    void OnClientPacket(AsyncTCPConnection* conn, Packet& pkt) {
        (void)conn;
        // Gateway -> Realtime：解析客户端消息，投递到 Compute Thread
        switch (pkt.header.cmd_id) {
            case CMD_REALTIME_ENTER: {
                // payload: [room_id: 4 BE][player_id: 8 BE][spawn_x: 4 BE][spawn_z: 4 BE]
                if (pkt.payload.Size() < 20) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data());
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 4);
                float spawn_x = *reinterpret_cast<const float*>(pkt.payload.Data() + 12);
                float spawn_z = *reinterpret_cast<const float*>(pkt.payload.Data() + 16);
                auto msg = std::make_unique<PlayerEnterMsg>();
                msg->player_id = player_id;
                msg->spawn_pos = {spawn_x, 0, spawn_z};
                msg->conn_id = pkt.header.seq_id; // 复用 seq_id 作为 conn_id（简化）
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_LEAVE: {
                if (pkt.payload.Size() < 12) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data());
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 4);
                auto msg = std::make_unique<PlayerLeaveMsg>();
                msg->player_id = player_id;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_MOVE: {
                if (pkt.payload.Size() < 24) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data());
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 4);
                float x = *reinterpret_cast<const float*>(pkt.payload.Data() + 12);
                float z = *reinterpret_cast<const float*>(pkt.payload.Data() + 16);
                float yaw = *reinterpret_cast<const float*>(pkt.payload.Data() + 20);
                auto msg = std::make_unique<PlayerMoveMsg>();
                msg->player_id = player_id;
                msg->target_pos = {x, 0, z};
                msg->target_yaw = yaw;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_ACTION: {
                if (pkt.payload.Size() < 20) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data());
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 4);
                uint32_t action_id = ReadU32BE(pkt.payload.Data() + 12);
                float x = *reinterpret_cast<const float*>(pkt.payload.Data() + 16);
                float z = *reinterpret_cast<const float*>(pkt.payload.Data() + 20);
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

    // Room 广播：将 Snapshot 编码为 Packet，通过 Gateway 已有连接发送
    // 每个 target_conn 对应一个定向包（seq_id = conn_id），由 Gateway 转发给对应客户端
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

        std::lock_guard<std::mutex> lk(conn_mtx_);
        // 为每个 target_conn 发送定向包；通过所有 Gateway 连接广播，
        // Gateway 会根据 seq_id（即 conn_id）转发给对应客户端
        for (uint64_t conn_id : target_conns) {
            Packet pkt;
            pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(payload.size());
            pkt.header.magic = MAGIC_VALUE;
            pkt.header.cmd_id = CMD_REALTIME_SYNC;
            pkt.header.seq_id = static_cast<uint32_t>(conn_id);
            pkt.header.flags = static_cast<uint32_t>(Flag::BROADCAST);
            pkt.payload = Buffer::FromVector(payload);
            for (const auto& [_, conn] : conns_) {
                (void)_;
                conn->SendPacket(pkt);
            }
        }
    }

    std::shared_ptr<gs::logger::Logger> logger_;
    std::unique_ptr<AsyncTCPServer> server_;
    std::unique_ptr<gs::discovery::ServiceDiscovery> sd_;
    std::unique_ptr<ComputeThread> compute_;

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> conns_;

    // 心跳线程
    std::thread heartbeat_thread_;
    std::atomic<bool> heartbeat_stop_{false};

    std::mutex stop_mtx_;
    std::condition_variable stop_cv_;
    bool stop_flag_ = false;
};

int main(int argc, char* argv[]) {
    uint16_t port = 8084;
    std::string discovery_type = "registry";
    std::vector<std::string> discovery_addrs = {"127.0.0.1:2379"};

    std::string log_file;
    std::string log_level = "info";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log-file" && i + 1 < argc) {
            log_file = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--discovery-type" && i + 1 < argc) {
            discovery_type = argv[++i];
        } else if (arg == "--discovery-addrs" && i + 1 < argc) {
            // 解析逗号分隔的地址列表
            std::string addrs = argv[++i];
            size_t pos = 0;
            while (pos < addrs.size()) {
                size_t comma = addrs.find(',', pos);
                if (comma == std::string::npos) comma = addrs.size();
                discovery_addrs.push_back(addrs.substr(pos, comma - pos));
                pos = comma + 1;
            }
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
    }

    auto logger = std::make_shared<gs::logger::Logger>("realtime", "realtime-1");
    logger->SetLevel(gs::logger::ParseLogLevel(log_level));
    if (!log_file.empty()) {
        logger->SetOutputFile(log_file);
    }

    RealtimeServer srv;
    srv.SetLogger(logger);
    if (!srv.Start(port, discovery_type, discovery_addrs)) {
        return 1;
    }
    // 注册 OS 信号处理（简化版）
    signal(SIGINT, [](int) {});
    signal(SIGTERM, [](int) {});

    srv.Wait();
    srv.Stop();
    return 0;
}
