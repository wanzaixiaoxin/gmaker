#include "redis_client.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "[C++ Redis Client Test]" << std::endl;

    gs::redis::Config cfg;
    cfg.Addrs = {"127.0.0.1:6379"};
    cfg.DB = 0;

    gs::redis::Client client(cfg);
    if (!client.IsConnected()) {
        std::cout << "WARN: cannot connect to redis (expected if redis not running)" << std::endl;
        std::cout << "Error: " << client.LastError() << std::endl;
        // 连接失败时仍然返回 0，因为环境可能没有 Redis
        return 0;
    }

    // Ping
    if (!client.Ping()) {
        std::cerr << "Ping failed: " << client.LastError() << std::endl;
        return 1;
    }
    std::cout << "Ping OK" << std::endl;

    // Set
    if (!client.Set("test:cpp:key1", "hello", 10)) {
        std::cerr << "Set failed: " << client.LastError() << std::endl;
        return 1;
    }
    std::cout << "Set OK" << std::endl;

    // Get
    auto val = client.Get("test:cpp:key1");
    if (!val || *val != "hello") {
        std::cerr << "Get failed or value mismatch" << std::endl;
        return 1;
    }
    std::cout << "Get OK: " << *val << std::endl;

    // Del
    if (!client.Del("test:cpp:key1")) {
        std::cerr << "Del failed: " << client.LastError() << std::endl;
        return 1;
    }
    std::cout << "Del OK" << std::endl;

    // Get after del
    val = client.Get("test:cpp:key1");
    if (val.has_value()) {
        std::cerr << "Get after del should return null" << std::endl;
        return 1;
    }
    std::cout << "Get after Del OK (nil)" << std::endl;

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
