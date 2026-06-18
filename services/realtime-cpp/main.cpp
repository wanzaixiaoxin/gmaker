#include <iostream>
#include <unordered_map>
#include <unordered_set>
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
#include "realtime/compute_pool.hpp"
#include "realtime/message.hpp"
#include "realtime/battle_types.hpp"
#include "realtime/battle_room.hpp"
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
constexpr uint32_t CMD_REALTIME_INPUT  = 0x00020022; // 客户端轻量状态同步
constexpr uint32_t CMD_REALTIME_SYNC   = protocol::CMD_RT_STATE_SYNC;

// MOBA 战斗命令
constexpr uint32_t CMD_BATTLE_READY     = 0x00020030; // 加载完成
constexpr uint32_t CMD_BATTLE_MOVE      = 0x00020031; // 英雄移动输入
constexpr uint32_t CMD_BATTLE_CAST      = 0x00020032; // 释放技能
constexpr uint32_t CMD_BATTLE_RECONNECT = 0x00020033; // 重连

// RealtimeServer 实时服
// Gateway 主动连接到 Realtime，Realtime 通过已有连接回推 Snapshot
struct RealtimeServer {
    void SetLogger(std::shared_ptr<gs::logger::Logger> logger) { logger_ = logger; }
    void SetBattleMode(bool enabled) { battle_mode_ = enabled; }

private:
    bool battle_mode_ = false; // MOBA 战斗模式

public:
    bool Start(uint16_t listen_port,
               const std::string& discovery_type,
               const std::vector<std::string>& discovery_addrs) {

        // 启动 metrics
        gs::metrics::ServeDefaultHTTP(":9090");

        // 初始化 Compute Thread
        compute_ = std::make_unique<ComputePool>(4);
        compute_->SetOutputCallback([this](uint32_t room_id, const RoomSnapshot& snap, const std::vector<uint64_t>& conns) {
            (void)room_id;
            OnRoomBroadcast(snap, conns);
        });

        // 设置 BattleRoom 工厂（MOBA 战斗模式）
        compute_->SetRoomFactory([](const RoomConfig& cfg) -> std::unique_ptr<gs::realtime::Room> {
            BattleRoomConfig bcfg;
            bcfg.base = cfg;
            bcfg.team_size = 5;
            bcfg.minion_spawn_interval_sec = 30;
            bcfg.checkpoint_interval_frames = 60;
            bcfg.max_reconnect_wait_sec = 60;
            bcfg.lockstep_timeout_ms = 200;

            // 蓝方出生点（左下）
            bcfg.blue_spawn_pts = {{10, 0, 10}, {15, 0, 10}, {10, 0, 15}, {15, 0, 15}, {12, 0, 12}};
            // 红方出生点（右上）
            bcfg.red_spawn_pts = {{90, 0, 90}, {85, 0, 90}, {90, 0, 85}, {85, 0, 85}, {88, 0, 88}};

            return std::make_unique<BattleRoom>(bcfg);
        });

        if (battle_mode_) {
            // MOBA 战斗模式：创建 BattleRoom
            for (uint32_t i = 1; i <= 3; ++i) {
                RoomConfig cfg;
                cfg.room_id = i;
                cfg.max_players = 10;
                cfg.map_size_x = 100.0f;
                cfg.map_size_z = 100.0f;
                cfg.tick_rate_hz = 60;
                cfg.enable_aoi = true;
                cfg.aoi_radius = 50.0f;
                compute_->CreateRoom(cfg);
                if (logger_) logger_->Info("Created battle room " + std::to_string(i));
            }
        } else {
            // 普通房间模式
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
    void HeartbeatLoop() {
        // 心跳已由 discovery::RegistryImpl 内部自动管理，此处保留空实现
        while (!heartbeat_stop_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    void OnClientConnect(AsyncTCPConnection* conn) {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_[conn->ID()] = conn;
        if (logger_) logger_->Info("Gateway connected: " + std::to_string(conn->ID()));
    }

    void OnClientPacket(AsyncTCPConnection* conn, Packet& pkt) {
        if (pkt.payload.Size() >= 8) {
            BindClientRoute(ReadU64BE(pkt.payload.Data()), conn);
        }

        // Gateway -> Realtime：解析客户端消息，投递到 Compute Thread
        switch (pkt.header.cmd_id) {
            case CMD_REALTIME_ENTER: {
                if (pkt.payload.Size() < 28) return;
                uint64_t gw_conn_id = ReadU64BE(pkt.payload.Data());
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                float spawn_x = ReadF32BE(pkt.payload.Data() + 20);
                float spawn_z = ReadF32BE(pkt.payload.Data() + 24);

                // 客户端按匹配服下发的 room_id 进入，缺失时由实时服分配兜底房间。
                if (room_id == 0) {
                    room_id = 1;
                }
                {
                    RoomConfig cfg;
                    cfg.room_id = room_id;
                    cfg.max_players = battle_mode_ ? 10 : 20;
                    cfg.map_size_x = battle_mode_ ? 100.0f : 1000.0f;
                    cfg.map_size_z = battle_mode_ ? 100.0f : 1000.0f;
                    cfg.tick_rate_hz = 60;
                    cfg.enable_aoi = true;
                    cfg.aoi_radius = battle_mode_ ? 50.0f : 200.0f;
                    if (compute_->CreateRoom(cfg) && logger_) {
                        logger_->Info("Dynamic room created: " + std::to_string(room_id));
                    }
                }

                auto msg = std::make_unique<PlayerEnterMsg>();
                msg->player_id = player_id;
                msg->spawn_pos = {spawn_x, 0, spawn_z};
                msg->conn_id = gw_conn_id;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_LEAVE: {
                if (pkt.payload.Size() < 20) return;
                uint64_t gw_conn_id = ReadU64BE(pkt.payload.Data());
                (void)gw_conn_id;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                auto msg = std::make_unique<PlayerLeaveMsg>();
                msg->player_id = player_id;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_MOVE: {
                if (pkt.payload.Size() < 32) return;
                uint64_t gw_conn_id = ReadU64BE(pkt.payload.Data());
                (void)gw_conn_id;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                float x = ReadF32BE(pkt.payload.Data() + 20);
                float z = ReadF32BE(pkt.payload.Data() + 24);
                float yaw = ReadF32BE(pkt.payload.Data() + 28);
                auto msg = std::make_unique<PlayerMoveMsg>();
                msg->player_id = player_id;
                msg->target_pos = {x, 0, z};
                msg->target_yaw = yaw;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_ACTION: {
                if (pkt.payload.Size() < 32) return;
                uint64_t gw_conn_id = ReadU64BE(pkt.payload.Data());
                (void)gw_conn_id;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                uint32_t action_id = ReadU32BE(pkt.payload.Data() + 20);
                float x = ReadF32BE(pkt.payload.Data() + 24);
                float z = ReadF32BE(pkt.payload.Data() + 28);
                auto msg = std::make_unique<PlayerActionMsg>();
                msg->player_id = player_id;
                msg->action_id = action_id;
                msg->target_pos = {x, 0, z};
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_REALTIME_INPUT: {
                if (pkt.payload.Size() < 12) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                auto msg = std::make_unique<RoomBroadcastMsg>();
                msg->payload.assign(pkt.payload.Data() + 12, pkt.payload.Data() + pkt.payload.Size());
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            default:
                break;
        }

        // MOBA 战斗消息
        switch (pkt.header.cmd_id) {
            case CMD_BATTLE_READY: {
                if (pkt.payload.Size() < 20) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                auto msg = std::make_unique<BattleReadyMsg>();
                msg->player_id = player_id;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_BATTLE_MOVE: {
                if (pkt.payload.Size() < 32) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                float move_x = ReadF32BE(pkt.payload.Data() + 20);
                float move_z = ReadF32BE(pkt.payload.Data() + 24);
                uint32_t input_seq = ReadU32BE(pkt.payload.Data() + 28);
                auto msg = std::make_unique<HeroMoveInputMsg>();
                msg->player_id = player_id;
                msg->move_x = move_x;
                msg->move_z = move_z;
                msg->input_seq = input_seq;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_BATTLE_CAST: {
                if (pkt.payload.Size() < 41) return;
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                uint8_t skill_slot = pkt.payload.Data()[20];
                float target_x = ReadF32BE(pkt.payload.Data() + 21);
                float target_z = ReadF32BE(pkt.payload.Data() + 25);
                uint64_t target_eid = ReadU64BE(pkt.payload.Data() + 29);
                uint32_t input_seq = ReadU32BE(pkt.payload.Data() + 37);
                auto msg = std::make_unique<HeroCastSkillMsg>();
                msg->player_id = player_id;
                msg->skill_slot = skill_slot;
                msg->target_pos = {target_x, 0, target_z};
                msg->target_entity_id = target_eid;
                msg->input_seq = input_seq;
                compute_->PushMessage(room_id, std::move(msg));
                break;
            }
            case CMD_BATTLE_RECONNECT: {
                if (pkt.payload.Size() < 24) return;
                uint64_t gw_conn_id = ReadU64BE(pkt.payload.Data());
                uint32_t room_id = ReadU32BE(pkt.payload.Data() + 8);
                uint64_t player_id = ReadU64BE(pkt.payload.Data() + 12);
                auto msg = std::make_unique<PlayerReconnectMsg>();
                msg->player_id = player_id;
                msg->conn_id = gw_conn_id;
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
        for (auto it = client_routes_.begin(); it != client_routes_.end(); ) {
            if (it->second == conn) {
                it = client_routes_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void BindClientRoute(uint64_t client_conn_id, AsyncTCPConnection* gateway_conn) {
        if (client_conn_id == 0 || gateway_conn == nullptr) return;
        std::lock_guard<std::mutex> lk(conn_mtx_);
        client_routes_[client_conn_id] = gateway_conn;
    }

    // Room 广播：将 Snapshot 编码为 Packet，通过 Gateway 已有连接发送
    // 每个 target_conn 对应一个定向包（seq_id = conn_id），由 Gateway 转发给对应客户端
    void OnRoomBroadcast(const RoomSnapshot& snap, const std::vector<uint64_t>& target_conns) {
        // 编码 RoomSnapshot 为二进制 payload
        // [frame_seq: 4 BE][timestamp: 8 BE][player_count: 4 BE]
        // 每个玩家: [player_id: 8 BE][x: 4][y: 4][z: 4][yaw: 4][hp: 4][anim: 4]
        std::vector<uint8_t> payload;
        auto append_u32 = [&payload](uint32_t v) {
            payload.resize(payload.size() + 4);
            WriteU32BE(&payload[payload.size() - 4], v);
        };
        auto append_u64 = [&payload](uint64_t v) {
            payload.resize(payload.size() + 8);
            WriteU64BE(&payload[payload.size() - 8], v);
        };
        if (!snap.raw_payload.empty()) {
            payload = snap.raw_payload;
        } else {
            payload.reserve(16 + snap.players.size() * 32);
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
        }

        if (target_conns.empty()) return;

        // 构建目标集合
        std::unordered_set<uint64_t> target_set(target_conns.begin(), target_conns.end());

        std::lock_guard<std::mutex> lk(conn_mtx_);

        Packet pkt;
        pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(payload.size() + 8);
        pkt.header.magic = MAGIC_VALUE;
        pkt.header.cmd_id = CMD_REALTIME_SYNC;
        pkt.header.seq_id = 0;
        pkt.header.flags = static_cast<uint32_t>(Flag::RPC_RES);

        for (uint64_t conn_id : target_set) {
            auto it = client_routes_.find(conn_id);
            if (it != client_routes_.end() && it->second != nullptr) {
                std::vector<uint8_t> routed_payload;
                routed_payload.reserve(8 + payload.size());
                routed_payload.resize(8);
                WriteU64BE(routed_payload.data(), conn_id);
                routed_payload.insert(routed_payload.end(), payload.begin(), payload.end());
                pkt.payload = Buffer::FromVector(std::move(routed_payload));
                it->second->SendPacket(pkt);
            } else if (logger_) {
                logger_->Warn("No gateway route for realtime client conn " + std::to_string(conn_id));
            }
        }
    }

    std::shared_ptr<gs::logger::Logger> logger_;
    std::unique_ptr<AsyncTCPServer> server_;
    std::unique_ptr<gs::discovery::ServiceDiscovery> sd_;
    std::unique_ptr<ComputePool> compute_;

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> conns_;
    std::unordered_map<uint64_t, AsyncTCPConnection*> client_routes_;

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
    bool battle_mode = false;

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
        } else if (arg == "--battle-mode") {
            battle_mode = true;
        }
    }

    auto logger = std::make_shared<gs::logger::Logger>("realtime", "realtime-1");
    logger->SetLevel(gs::logger::ParseLogLevel(log_level));
    if (!log_file.empty()) {
        logger->SetOutputFile(log_file);
    }

    RealtimeServer srv;
    srv.SetLogger(logger);
    srv.SetBattleMode(battle_mode);
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
