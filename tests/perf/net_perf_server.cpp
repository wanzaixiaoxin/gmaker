// net_perf_server.cpp — 独立服务端（Echo + Broadcast）
//
// 用法: test-net-perf-server <port>
//
// 支持命令：
//   - Echo: 收到 cmd_id=0xE001 的包原样发回
//   - Broadcast: 每 100ms 自动广播一次（如果开启）
//   - 统计: Ctrl+C 或进程退出时打印收发统计

#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <fstream>
#include <exception>

#include "gs/net/packet.hpp"
#include "gs/net/async/tcp_server.hpp"

using namespace gs::net;
using namespace gs::net::async;

static std::atomic<bool> g_running{true};

static void LogCrash(const char* msg) {
    std::ofstream f("server_crash_dump.log", std::ios::app);
    f << "[" << std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count()
      << "] CRASH: " << msg << std::endl;
    f.flush();
}

#ifdef _WIN32
static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* ep) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Unhandled exception code=0x%08X at address=0x%p",
             ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);
    LogCrash(buf);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void TerminateHandler() {
    LogCrash("std::terminate called");
    std::abort();
}
static std::atomic<uint64_t> g_packets_received{0};
static std::atomic<uint64_t> g_packets_echoed{0};
static std::atomic<uint64_t> g_packets_broadcast{0};
static std::atomic<uint64_t> g_bytes_received{0};
static std::atomic<uint64_t> g_connections{0};

static AsyncTCPServer* g_server_ptr = nullptr;

void SignalHandler(int) {
    g_running.store(false);
    if (g_server_ptr) {
        g_server_ptr->Stop();
    }
}

Buffer MakeBroadcastPacket(int seq_id, int payload_size) {
    std::vector<uint8_t> payload(payload_size);
    WriteU32BE(payload.data(), static_cast<uint32_t>(seq_id));
    for (size_t i = 4; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(seq_id + i);
    }
    Packet pkt;
    pkt.header.magic  = MAGIC_VALUE;
    pkt.header.cmd_id = 0xB001;
    pkt.header.seq_id = static_cast<uint32_t>(seq_id);
    pkt.header.flags  = 0;
    pkt.payload = Buffer::FromVector(std::move(payload));
    pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(payload.size());
    return EncodePacket(pkt);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(ExceptionFilter);
#endif
    std::set_terminate(TerminateHandler);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [broadcast_interval_ms] [broadcast_payload_size]" << std::endl;
        std::cerr << "  port: listen port" << std::endl;
        std::cerr << "  broadcast_interval_ms: 0=disable broadcast (default), >0=auto broadcast interval" << std::endl;
        std::cerr << "  broadcast_payload_size: broadcast payload size in bytes (default 256)" << std::endl;
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    int broadcast_interval_ms = (argc > 2) ? std::atoi(argv[2]) : 0;
    int broadcast_payload_size = (argc > 3) ? std::atoi(argv[3]) : 256;

    std::signal(SIGINT, SignalHandler);
#ifdef _WIN32
    std::signal(SIGBREAK, SignalHandler);
#else
    std::signal(SIGTERM, SignalHandler);
#endif

    AsyncTCPServer server({"0.0.0.0", port, 10000});
    g_server_ptr = &server;

    server.SetCallbacks(
        [](AsyncTCPConnection* conn) {
            g_connections.fetch_add(1);
            std::cout << "[CONN] New connection, id=" << conn->ID()
                      << ", total=" << g_connections.load() << std::endl;
        },
        [](AsyncTCPConnection* conn, Packet& pkt) {
            g_packets_received.fetch_add(1);
            g_bytes_received.fetch_add(pkt.header.length);

            if (pkt.header.cmd_id == 0xE001) {
                // Echo：使用框架标准接口 SendPacket
                conn->SendPacket(pkt);
                g_packets_echoed.fetch_add(1);
            }
        },
        [](AsyncTCPConnection* conn) {
            g_connections.fetch_sub(1);
            std::cout << "[DISC] Connection closed, id=" << conn->ID()
                      << ", remaining=" << g_connections.load() << std::endl;
        }
    );

    if (!server.Start()) {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return 1;
    }

    std::cout << "[SERVER] Listening on 0.0.0.0:" << port << std::endl;
    std::cout << "[SERVER] Echo enabled (cmd_id=0xE001)" << std::endl;
    if (broadcast_interval_ms > 0) {
        std::cout << "[SERVER] Auto-broadcast every " << broadcast_interval_ms
                  << "ms, payload=" << broadcast_payload_size << " bytes" << std::endl;
    }
    std::cout << "[SERVER] Press Ctrl+C to stop" << std::endl;

    // 自动广播线程（如果启用）
    std::thread broadcast_thread;
    if (broadcast_interval_ms > 0) {
        broadcast_thread = std::thread([&]() {
            int seq = 0;
            while (g_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(broadcast_interval_ms));
                if (!g_running.load()) break;

                Packet pkt;
                pkt.header.magic  = MAGIC_VALUE;
                pkt.header.cmd_id = 0xB001;
                pkt.header.seq_id = static_cast<uint32_t>(seq);
                pkt.header.flags  = 0;

                std::vector<uint8_t> payload(broadcast_payload_size);
                WriteU32BE(payload.data(), static_cast<uint32_t>(seq));
                for (size_t i = 4; i < payload.size(); ++i) {
                    payload[i] = static_cast<uint8_t>(seq + i);
                }
                pkt.payload = Buffer::FromVector(std::move(payload));
                pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(payload.size());

                server.Broadcast(pkt);
                g_packets_broadcast.fetch_add(1);
                seq++;
            }
        });
    }

    // 主线程：定期打印统计
    auto last_time = std::chrono::steady_clock::now();
    uint64_t last_recv = 0;
    uint64_t last_echo = 0;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - last_time).count();
        if (elapsed_sec >= 1.0) {
            uint64_t curr_recv = g_packets_received.load();
            uint64_t curr_echo = g_packets_echoed.load();
            uint64_t recv_delta = curr_recv - last_recv;
            uint64_t echo_delta = curr_echo - last_echo;

            std::cout << "[STAT] conn=" << g_connections.load()
                      << " recv=" << curr_recv << " (" << recv_delta << "/s)"
                      << " echo=" << curr_echo << " (" << echo_delta << "/s)"
                      << " broadcast=" << g_packets_broadcast.load()
                      << std::endl;

            last_recv = curr_recv;
            last_echo = curr_echo;
            last_time = now;
        }
    }

    if (broadcast_thread.joinable()) {
        broadcast_thread.join();
    }

    // 最终统计
    std::cout << "\n========== Server Final Statistics ==========" << std::endl;
    std::cout << "Total connections   : " << g_connections.load() << std::endl;
    std::cout << "Total packets recv  : " << g_packets_received.load() << std::endl;
    std::cout << "Total packets echoed: " << g_packets_echoed.load() << std::endl;
    std::cout << "Total packets broadcast: " << g_packets_broadcast.load() << std::endl;
    std::cout << "Total bytes received: " << g_bytes_received.load() << std::endl;

    server.Stop();
    g_server_ptr = nullptr;
    return 0;
}
