// net_perf_client.cpp — 独立客户端（正确性 + 性能测试）
//
// 用法:
//   correctness: test-net-perf-client correctness <host> <port> <clients> <packets> <payload_size>
//   throughput : test-net-perf-client throughput  <host> <port> <clients> <duration_ms> <payload_size>
//   latency    : test-net-perf-client latency     <host> <port> <samples> <payload_size>
//   broadcast  : test-net-perf-client broadcast   <host> <port> <clients> <duration_ms> <payload_size>

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <random>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "gs/net/packet.hpp"

using namespace gs::net;

static bool g_wsa_initialized = false;

bool InitNetwork() {
#ifdef _WIN32
    if (!g_wsa_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
        g_wsa_initialized = true;
    }
#endif
    return true;
}

void CleanupNetwork() {
#ifdef _WIN32
    if (g_wsa_initialized) { WSACleanup(); g_wsa_initialized = false; }
#endif
}

int CreateSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) return -1;
#else
    if (sock < 0) return -1;
#endif
    return sock;
}

void CloseSocket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

bool ConnectSocket(int sock, const std::string& host, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    return connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0;
}

bool SendAll(int sock, const uint8_t* data, size_t len, int timeout_ms = 30000) {
    size_t sent = 0;
    auto start = std::chrono::steady_clock::now();
    while (sent < len) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > timeout_ms) return false;
        int n = send(sock, (const char*)data + sent, (int)(len - sent), 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool RecvAll(int sock, uint8_t* buf, size_t len, int timeout_ms = 30000) {
    size_t received = 0;
    auto start = std::chrono::steady_clock::now();
    while (received < len) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > timeout_ms) return false;
        int n = recv(sock, (char*)buf + received, (int)(len - received), 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

Buffer BuildTestPacket(uint32_t client_id, uint32_t seq_id, int payload_size, std::mt19937& rng) {
    std::vector<uint8_t> payload(payload_size);
    WriteU32BE(payload.data(), client_id);
    WriteU32BE(payload.data() + 4, seq_id);
    for (size_t i = 8; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(rng());
    }
    Packet pkt;
    pkt.header.magic  = MAGIC_VALUE;
    pkt.header.cmd_id = 0xE001;
    pkt.header.seq_id = seq_id;
    pkt.header.flags  = 0;
    pkt.payload = Buffer::FromVector(std::move(payload));
    pkt.header.length = HEADER_SIZE + payload_size;
    return EncodePacket(pkt);
}

// ========================================================================
// 测试 1：Correctness
// ========================================================================

struct CorrectnessResult {
    bool success = true;
    std::string error;
    int packets_sent = 0;
    int packets_received = 0;
};

void CorrectnessClient(int client_id, const std::string& host, uint16_t port,
                       int num_packets, int payload_size,
                       std::atomic<int>& ready_count,
                       std::atomic<bool>& start_flag,
                       CorrectnessResult& out) {
    int sock = CreateSocket();
    if (sock < 0 || !ConnectSocket(sock, host, port)) {
        out.success = false; out.error = "connect failed";
        if (sock >= 0) CloseSocket(sock);
        ready_count.fetch_add(1);
        return;
    }

    std::mt19937 rng(static_cast<unsigned>(client_id * 7919 + 104729));
    std::vector<std::vector<uint8_t>> sent_packets(num_packets);

    ready_count.fetch_add(1);
    while (!start_flag.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    for (int seq = 0; seq < num_packets; ++seq) {
        auto buf = BuildTestPacket(client_id, seq, payload_size, rng);
        sent_packets[seq] = buf.ToVector();
        if (!SendAll(sock, buf.Data(), buf.Size(), 30000)) {
            out.success = false; out.error = "send failed seq=" + std::to_string(seq);
            break;
        }
        out.packets_sent++;
    }

    if (out.success) {
        std::vector<bool> received(num_packets, false);
        for (int i = 0; i < num_packets; ++i) {
            uint8_t header[HEADER_SIZE];
            if (!RecvAll(sock, header, HEADER_SIZE, 30000)) {
                out.success = false; out.error = "recv header timeout i=" + std::to_string(i);
                break;
            }
            uint32_t length = ReadU32BE(header);
            if (length < HEADER_SIZE || length > MAX_PACKET_LEN) {
                out.success = false; out.error = "invalid length";
                break;
            }
            uint32_t payload_len = length - HEADER_SIZE;
            std::vector<uint8_t> full(length);
            std::memcpy(full.data(), header, HEADER_SIZE);
            if (payload_len > 0) {
                if (!RecvAll(sock, full.data() + HEADER_SIZE, payload_len, 30000)) {
                    out.success = false; out.error = "recv payload timeout";
                    break;
                }
            }
            Header h = DecodeHeader(full.data());
            if (h.magic != MAGIC_VALUE || h.cmd_id != 0xE001) {
                out.success = false; out.error = "header mismatch";
                break;
            }
            if (payload_len < 8) {
                out.success = false; out.error = "payload too short";
                break;
            }
            uint32_t recv_cid = ReadU32BE(full.data() + HEADER_SIZE);
            uint32_t recv_seq = ReadU32BE(full.data() + HEADER_SIZE + 4);
            if (recv_cid != static_cast<uint32_t>(client_id)) {
                out.success = false;
                out.error = "client_id cross-talk! expected=" + std::to_string(client_id) +
                            " got=" + std::to_string(recv_cid);
                break;
            }
            if (recv_seq >= static_cast<uint32_t>(num_packets) || received[recv_seq]) {
                out.success = false;
                out.error = "seq invalid/duplicate: " + std::to_string(recv_seq);
                break;
            }
            received[recv_seq] = true;
            if (full.size() != sent_packets[recv_seq].size() ||
                std::memcmp(full.data(), sent_packets[recv_seq].data(), full.size()) != 0) {
                out.success = false;
                out.error = "payload mismatch seq=" + std::to_string(recv_seq);
                break;
            }
            out.packets_received++;
        }
    }
    CloseSocket(sock);
}

bool TestCorrectness(const std::string& host, uint16_t port,
                     int num_clients, int num_packets, int payload_size) {
    std::cout << "\n========== Correctness Test ==========" << std::endl;
    std::cout << "Host: " << host << ":" << port
              << " | Clients: " << num_clients
              << " | Packets/client: " << num_packets
              << " | Payload: " << payload_size << " bytes" << std::endl;

    std::atomic<int> ready_count{0};
    std::atomic<bool> start_flag{false};
    std::vector<CorrectnessResult> results(num_clients);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(CorrectnessClient, i, host, port, num_packets, payload_size,
                             std::ref(ready_count), std::ref(start_flag), std::ref(results[i]));
    }

    while (ready_count.load() < num_clients) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto t0 = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    for (auto& t : threads) t.join();
    auto t1 = std::chrono::high_resolution_clock::now();

    bool all_ok = true;
    int total_sent = 0, total_recv = 0;
    for (int i = 0; i < num_clients; ++i) {
        total_sent += results[i].packets_sent;
        total_recv += results[i].packets_received;
        if (!results[i].success) {
            all_ok = false;
            std::cerr << "[FAIL] Client " << i << ": " << results[i].error << std::endl;
        }
    }

    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    double packet_bytes = HEADER_SIZE + payload_size;
    double total_mb = total_sent * packet_bytes / (1024.0 * 1024.0);

    std::cout << "Sent: " << total_sent << " | Received: " << total_recv
              << " | Time: " << elapsed << "s"
              << " | Throughput: " << (total_mb / elapsed) << " MB/s" << std::endl;
    std::cout << (all_ok ? "[PASS]" : "[FAIL]") << std::endl;
    return all_ok;
}

// ========================================================================
// 测试 2：Throughput
// ========================================================================

struct ThroughputResult {
    int packets_sent = 0;
    double elapsed_sec = 0;
};

void ThroughputClient(const std::string& host, uint16_t port,
                      int duration_ms, int payload_size,
                      std::atomic<int>& ready_count,
                      std::atomic<bool>& start_flag,
                      ThroughputResult& out) {
    int sock = CreateSocket();
    if (sock < 0 || !ConnectSocket(sock, host, port)) {
        if (sock >= 0) CloseSocket(sock);
        ready_count.fetch_add(1);
        return;
    }
    std::mt19937 rng(12345);
    ready_count.fetch_add(1);
    while (!start_flag.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // 启动后台读取线程，不断读取并丢弃服务器的 Echo 数据，避免 TCP 窗口阻塞发送
    std::atomic<bool> stop_read{false};
    std::thread reader([&sock, &stop_read]() {
        while (!stop_read.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            timeval tv{};
            tv.tv_usec = 10000; // 10ms
            int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);
            if (ret > 0 && FD_ISSET(sock, &fds)) {
                uint8_t buf[8192];
                int n = recv(sock, (char*)buf, sizeof(buf), 0);
                if (n <= 0) break;
            } else if (ret < 0) {
                break;
            }
        }
    });

    auto t_start = std::chrono::high_resolution_clock::now();
    int seq = 0;
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count();
        if (elapsed >= duration_ms) break;
        auto buf = BuildTestPacket(0, seq++, payload_size, rng);
        if (!SendAll(sock, buf.Data(), buf.Size(), 5000)) break;
        out.packets_sent++;
    }
    out.elapsed_sec = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - t_start).count();

    stop_read.store(true);
    // 强制关闭 socket 以打断可能阻塞在 select/recv 的读取线程
    CloseSocket(sock);
    if (reader.joinable()) reader.join();
}

bool TestThroughput(const std::string& host, uint16_t port,
                    int num_clients, int duration_ms, int payload_size) {
    std::cout << "\n========== Throughput Test ==========" << std::endl;
    std::cout << "Host: " << host << ":" << port
              << " | Clients: " << num_clients
              << " | Duration: " << duration_ms << "ms"
              << " | Payload: " << payload_size << " bytes" << std::endl;

    std::atomic<int> ready_count{0};
    std::atomic<bool> start_flag{false};
    std::vector<ThroughputResult> results(num_clients);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(ThroughputClient, host, port, duration_ms, payload_size,
                             std::ref(ready_count), std::ref(start_flag), std::ref(results[i]));
    }
    while (ready_count.load() < num_clients) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    for (auto& t : threads) t.join();
    auto t1 = std::chrono::high_resolution_clock::now();

    int total_sent = 0;
    for (auto& r : results) total_sent += r.packets_sent;
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    double packet_bytes = HEADER_SIZE + payload_size;
    double mbps = (total_sent * packet_bytes * 8.0) / (elapsed * 1024.0 * 1024.0);
    double pps = total_sent / elapsed;

    std::cout << "Total sent: " << total_sent << " packets" << std::endl;
    std::cout << "Elapsed: " << elapsed << " s" << std::endl;
    std::cout << "PPS: " << pps << " packets/s" << std::endl;
    std::cout << "Bandwidth: " << mbps << " Mbps" << std::endl;
    return true;
}

// ========================================================================
// 测试 3：Latency (Ping-Pong RTT)
// ========================================================================

bool TestLatency(const std::string& host, uint16_t port, int samples, int payload_size) {
    std::cout << "\n========== Latency Test ==========" << std::endl;
    std::cout << "Host: " << host << ":" << port
              << " | Samples: " << samples
              << " | Payload: " << payload_size << " bytes" << std::endl;

    int sock = CreateSocket();
    if (sock < 0 || !ConnectSocket(sock, host, port)) {
        std::cerr << "Connect failed" << std::endl;
        if (sock >= 0) CloseSocket(sock);
        return false;
    }

    std::mt19937 rng(12345);
    std::vector<double> latencies;
    latencies.reserve(samples);

    for (int i = 0; i < samples; ++i) {
        auto buf = BuildTestPacket(0, i, payload_size, rng);
        auto t_send = std::chrono::high_resolution_clock::now();
        if (!SendAll(sock, buf.Data(), buf.Size(), 5000)) {
            std::cerr << "Send failed at sample " << i << std::endl;
            break;
        }

        uint8_t header[HEADER_SIZE];
        if (!RecvAll(sock, header, HEADER_SIZE, 5000)) {
            std::cerr << "Recv header failed at sample " << i << std::endl;
            break;
        }
        uint32_t length = ReadU32BE(header);
        uint32_t payload_len = length - HEADER_SIZE;
        if (payload_len > 0) {
            std::vector<uint8_t> dummy(payload_len);
            if (!RecvAll(sock, dummy.data(), payload_len, 5000)) {
                std::cerr << "Recv payload failed at sample " << i << std::endl;
                break;
            }
        }
        auto t_recv = std::chrono::high_resolution_clock::now();
        latencies.push_back(std::chrono::duration<double, std::milli>(t_recv - t_send).count());
    }
    CloseSocket(sock);

    if (latencies.empty()) return false;

    std::sort(latencies.begin(), latencies.end());
    double min_lat = latencies.front();
    double max_lat = latencies.back();
    double avg_lat = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double p50 = latencies[latencies.size() * 50 / 100];
    double p99 = latencies[latencies.size() * 99 / 100];

    std::cout << "Samples: " << latencies.size() << std::endl;
    std::cout << "Min: " << min_lat << " ms" << std::endl;
    std::cout << "Avg: " << avg_lat << " ms" << std::endl;
    std::cout << "P50: " << p50 << " ms" << std::endl;
    std::cout << "P99: " << p99 << " ms" << std::endl;
    std::cout << "Max: " << max_lat << " ms" << std::endl;
    return true;
}

// ========================================================================
// 测试 4：Broadcast 接收
// ========================================================================

struct BroadcastResult {
    int packets_received = 0;
    bool success = true;
    std::string error;
};

void BroadcastClient(const std::string& host, uint16_t port, int duration_ms,
                     int payload_size, std::atomic<int>& ready_count,
                     std::atomic<bool>& start_flag, BroadcastResult& out) {
    int sock = CreateSocket();
    if (sock < 0 || !ConnectSocket(sock, host, port)) {
        if (sock >= 0) CloseSocket(sock);
        ready_count.fetch_add(1);
        return;
    }
    ready_count.fetch_add(1);
    while (!start_flag.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto t_start = std::chrono::high_resolution_clock::now();
    uint32_t expected_bseq = 0;
    bool first_packet = true;
    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count();
        if (elapsed >= duration_ms) break;

        // 使用 select 超时检查，避免 recv 永久阻塞导致无法按时退出
        int remaining_ms = static_cast<int>(duration_ms - elapsed);
        if (remaining_ms <= 0) break;
        int timeout_ms = (std::min)(remaining_ms, 100);

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(sock + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) { out.error = "select error"; out.success = false; break; }
        if (ret == 0) continue; // 超时，检查 duration 并继续

        uint8_t header[HEADER_SIZE];
        int n = recv(sock, (char*)header, HEADER_SIZE, 0);
        if (n <= 0) {
            if (n < 0) { out.error = "recv error"; out.success = false; }
            break;
        }
        if (n < HEADER_SIZE) {
            int remain = HEADER_SIZE - n;
            int n2 = recv(sock, (char*)header + n, remain, 0);
            if (n2 <= 0) { out.error = "recv partial header"; out.success = false; break; }
        }
        uint32_t length = ReadU32BE(header);
        if (length < HEADER_SIZE || length > MAX_PACKET_LEN) {
            out.error = "invalid length"; out.success = false; break;
        }
        uint32_t payload_len = length - HEADER_SIZE;
        if (payload_len > 0) {
            std::vector<uint8_t> dummy(payload_len);
            if (!RecvAll(sock, dummy.data(), payload_len, 5000)) {
                out.error = "recv payload timeout"; out.success = false; break;
            }
            // 验证广播 payload（seq 只需连续递增，不要求从 0 开始）
            if (payload_len >= 4) {
                uint32_t bseq = ReadU32BE(dummy.data());
                if (first_packet) {
                    expected_bseq = bseq;
                    first_packet = false;
                }
                if (bseq != expected_bseq) {
                    out.error = "broadcast seq mismatch: expected=" +
                                std::to_string(expected_bseq) +
                                " got=" + std::to_string(bseq);
                    out.success = false;
                    break;
                }
                expected_bseq++;
            }
        }
        out.packets_received++;
    }
    CloseSocket(sock);
}

bool TestBroadcast(const std::string& host, uint16_t port,
                   int num_clients, int duration_ms, int payload_size) {
    std::cout << "\n========== Broadcast Receive Test ==========" << std::endl;
    std::cout << "Host: " << host << ":" << port
              << " | Clients: " << num_clients
              << " | Duration: " << duration_ms << "ms"
              << " | Payload: " << payload_size << " bytes" << std::endl;
    std::cout << "(Make sure server is running with broadcast enabled)" << std::endl;

    std::atomic<int> ready_count{0};
    std::atomic<bool> start_flag{false};
    std::vector<BroadcastResult> results(num_clients);
    std::vector<std::thread> threads;

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back(BroadcastClient, host, port, duration_ms, payload_size,
                             std::ref(ready_count), std::ref(start_flag), std::ref(results[i]));
    }
    while (ready_count.load() < num_clients) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto t0 = std::chrono::high_resolution_clock::now();
    start_flag.store(true);
    for (auto& t : threads) t.join();
    auto t1 = std::chrono::high_resolution_clock::now();

    bool all_ok = true;
    int total_recv = 0;
    int min_recv = INT_MAX;
    int max_recv = 0;
    for (int i = 0; i < num_clients; ++i) {
        total_recv += results[i].packets_received;
        min_recv = std::min(min_recv, results[i].packets_received);
        max_recv = std::max(max_recv, results[i].packets_received);
        if (!results[i].success) {
            all_ok = false;
            std::cerr << "[FAIL] Client " << i << ": " << results[i].error << std::endl;
        }
    }
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "Total received: " << total_recv << " | Min/client: " << min_recv
              << " | Max/client: " << max_recv << " | Time: " << elapsed << "s" << std::endl;
    std::cout << (all_ok ? "[PASS]" : "[FAIL]") << std::endl;
    return all_ok;
}

// ========================================================================
// main
// ========================================================================

void PrintUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " correctness <host> <port> <clients> <packets> <payload_size>\n"
              << "  " << prog << " throughput  <host> <port> <clients> <duration_ms> <payload_size>\n"
              << "  " << prog << " latency     <host> <port> <samples> <payload_size>\n"
              << "  " << prog << " broadcast   <host> <port> <clients> <duration_ms> <payload_size>\n"
              << "\nExamples:\n"
              << "  " << prog << " correctness 127.0.0.1 19999 50 500 256\n"
              << "  " << prog << " throughput  127.0.0.1 19999 10 5000 256\n"
              << "  " << prog << " latency     127.0.0.1 19999 1000 256\n"
              << "  " << prog << " broadcast   127.0.0.1 19999 100 10000 256\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    if (!InitNetwork()) {
        std::cerr << "Network init failed" << std::endl;
        return 1;
    }

    bool ok = false;
    if (mode == "correctness" && argc == 7) {
        ok = TestCorrectness(argv[2], static_cast<uint16_t>(std::atoi(argv[3])),
                             std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]));
    } else if (mode == "throughput" && argc == 7) {
        ok = TestThroughput(argv[2], static_cast<uint16_t>(std::atoi(argv[3])),
                            std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]));
    } else if (mode == "latency" && argc == 6) {
        ok = TestLatency(argv[2], static_cast<uint16_t>(std::atoi(argv[3])),
                         std::atoi(argv[4]), std::atoi(argv[5]));
    } else if (mode == "broadcast" && argc == 7) {
        ok = TestBroadcast(argv[2], static_cast<uint16_t>(std::atoi(argv[3])),
                           std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]));
    } else {
        PrintUsage(argv[0]);
        CleanupNetwork();
        return 1;
    }

    CleanupNetwork();
    return ok ? 0 : 1;
}
