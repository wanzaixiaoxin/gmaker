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
#include "gs/net/tcp_client.hpp"
#include "gs/net/packet.hpp"

using namespace gs::net;

class Gateway {
public:
    bool Start(uint16_t listen_port, const std::string& biz_host, uint16_t biz_port) {
        // 连接到 Biz 后端（带重试）
        biz_client_ = std::make_unique<TCPClient>(biz_host, biz_port);
        biz_client_->SetCallbacks(
            [this](TCPConn* c, Packet& p) { OnBizPacket(c, p); },
            [this](TCPConn* c) { std::cerr << "Biz connection closed" << std::endl; }
        );
        bool connected = false;
        for (int i = 0; i < 10; ++i) {
            if (biz_client_->Connect()) {
                connected = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (!connected) {
            std::cerr << "Failed to connect to biz backend" << std::endl;
            return false;
        }
        std::cout << "Connected to biz backend " << biz_host << ":" << biz_port << std::endl;

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
        if (biz_client_) biz_client_->Close();
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
        std::lock_guard<std::mutex> lk(clients_mtx_);
        clients_[conn->ID()] = conn;
        std::cout << "Client connected: " << conn->ID() << std::endl;
    }

    void OnClientPacket(TCPConn* conn, Packet& pkt) {
        (void)conn;
        // 转发到 Biz
        if (biz_client_ && biz_client_->Conn()) {
            biz_client_->Conn()->SendPacket(pkt);
        }
    }

    void OnClientClose(TCPConn* conn) {
        std::lock_guard<std::mutex> lk(clients_mtx_);
        clients_.erase(conn->ID());
        std::cout << "Client disconnected: " << conn->ID() << std::endl;
    }

    void OnBizPacket(TCPConn* conn, Packet& pkt) {
        (void)conn;
        // 广播给所有客户端（Phase 1 简化版）
        std::lock_guard<std::mutex> lk(clients_mtx_);
        for (auto& [id, client] : clients_) {
            (void)id;
            client->SendPacket(pkt);
        }
    }

    std::unique_ptr<TCPServer> server_;
    std::unique_ptr<TCPClient> biz_client_;
    std::mutex clients_mtx_;
    std::unordered_map<uint64_t, TCPConn*> clients_;

    std::mutex stop_mtx_;
    std::condition_variable stop_cv_;
    bool stop_flag_ = false;
};

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif
    uint16_t gateway_port = 8081;
    std::string biz_host = "127.0.0.1";
    uint16_t biz_port = 8082;

    if (argc > 1) gateway_port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc > 2) biz_host = argv[2];
    if (argc > 3) biz_port = static_cast<uint16_t>(std::atoi(argv[3]));

    Gateway gw;
    if (!gw.Start(gateway_port, biz_host, biz_port)) {
        return 1;
    }
    gw.Wait();
    gw.Stop();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
