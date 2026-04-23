#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace gs {
namespace config {

// 服务通用配置
struct ServiceConfig {
    std::string service_type;
    std::string node_id;
    std::string log_level = "info";
    std::string log_file;
    std::string metrics_addr;
};

// 网络配置
struct NetworkConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    int max_connections = 10000;
};

// 上游节点
struct UpstreamNode {
    std::string host;
    uint16_t port = 0;
};

// 上游服务配置
struct UpstreamConfig {
    std::unordered_map<std::string, std::vector<UpstreamNode>> services;
    
    std::vector<UpstreamNode> GetServiceNodes(const std::string& service_type) const {
        auto it = services.find(service_type);
        if (it != services.end()) {
            return it->second;
        }
        return {};
    }
};

// 命令范围配置
struct CmdRangeConfig {
    uint32_t start = 0;
    uint32_t end = 0;
};

// 安全配置
struct SecurityConfig {
    std::string master_key_hex;
    int replay_window_seconds = 300;
};

// Registry 配置
struct RegistryConfig {
    std::vector<UpstreamNode> nodes;
};

} // namespace config
} // namespace gs
