#include <iostream>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include "../packet.hpp"
#include "event_loop.hpp"
#include "tcp_connection.hpp"
#include "tcp_listener.hpp"
#include "tcp_server.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

using namespace gs::net::async;
using namespace gs::net;

// 测试 1：EventLoop 初始化和停止
void TestEventLoopInit() {
    std::cout << "[Test] EventLoop Init ... " << std::flush;
    AsyncEventLoop loop;
    assert(loop.Init());
    assert(loop.RawLoop() != nullptr);
    std::cout << "OK" << std::endl;
}

// 测试 2：EventLoop Post 任务
void TestEventLoopPost() {
    std::cout << "[Test] EventLoop Post ... " << std::flush;
    AsyncEventLoop loop;
    assert(loop.Init());

    bool executed = false;
    loop.Post([&executed]() {
        executed = true;
    });

    // 在另一个线程运行事件循环，主线程等待任务执行
    std::thread t([&loop]() {
        loop.Run();
    });

    // 等待任务执行
    for (int i = 0; i < 100 && !executed; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    loop.Stop();
    t.join();
    assert(executed);
    std::cout << "OK" << std::endl;
}

// 测试 3：TCP Listener 绑定（不实际接受连接）
void TestTCPListenerBind() {
    std::cout << "[Test] TCP Listener Bind ... " << std::flush;
    AsyncEventLoop loop;
    assert(loop.Init());

    AsyncTCPListener listener(&loop);
    bool ok = listener.Listen("127.0.0.1", 19001);
    assert(ok);

    listener.Stop();
    std::cout << "OK" << std::endl;
}

// 测试 4：完整 Echo 服务器/客户端流程
void TestEchoServer() {
    std::cout << "[Test] Echo Server ... " << std::flush;

    AsyncEventLoop server_loop;
    assert(server_loop.Init());

    AsyncTCPListener listener(&server_loop);
    std::shared_ptr<AsyncTCPConnection> server_conn;
    bool server_received = false;

    listener.SetConnectionCallback([&server_conn, &server_received, &server_loop
    ](uv_tcp_t* client) {
        auto conn = std::make_shared<AsyncTCPConnection>(&server_loop, 1);
        if (!conn->InitFromAccepted(client)) {
            return;
        }
        conn->SetCallbacks(
            [&server_received](AsyncTCPConnection*, Packet& pkt) {
                if (pkt.header.cmd_id == 0x1234) {
                    server_received = true;
                }
            },
            [](AsyncTCPConnection*) {}
        );
        server_conn = conn;
    });

    bool listen_ok = listener.Listen("127.0.0.1", 19002);
    assert(listen_ok);

    std::thread server_thread([&server_loop]() {
        server_loop.Run();
    });

    // 等待服务器就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(sock >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(19002);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int r = connect(sock, (const sockaddr*)&addr, sizeof(addr));
    assert(r == 0);

    // 发送测试数据
    Packet pkt;
    pkt.header.magic = MAGIC_VALUE;
    pkt.header.cmd_id = 0x1234;
    pkt.header.seq_id = 1;
    pkt.header.flags = 0;
    pkt.payload = {0x01, 0x02, 0x03, 0x04};
    pkt.header.length = HEADER_SIZE + static_cast<uint32_t>(pkt.payload.size());

    auto data = EncodePacket(pkt);
    int sent = send(sock, reinterpret_cast<const char*>(data.data()),
                    static_cast<int>(data.size()), 0);
    assert(sent == static_cast<int>(data.size()));

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    // 等待服务器收到数据
    for (int i = 0; i < 100 && !server_received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(server_received);

    // 关闭服务器端（所有操作在 server_loop 线程执行）
    server_loop.Post([&]() {
        if (server_conn) server_conn->Close();
        listener.Stop();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server_loop.Stop();
    server_thread.join();

    std::cout << "OK" << std::endl;
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  Async Net (libuv) Unit Tests" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << std::endl;

    try {
        TestEventLoopInit();
        TestEventLoopPost();
        TestTCPListenerBind();
        TestEchoServer();
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  All tests PASSED" << std::endl;
    std::cout << "============================================" << std::endl;
    return 0;
}
