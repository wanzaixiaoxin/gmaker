#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace gs {
namespace gateway {

// Gateway 配置
struct Config {
    // 服务信息
    std::string service_type = "gateway";
    std::string node_id = "gateway-1";
    
    // 网络配置
    uint16_t listen_port = 8081;
    int max_connections = 10000;
    
    // Metrics 配置
    std::string metrics_addr = ":9081";
    
    // Registry 配置
    std::vector<std::string> registry_nodes;
    
    // 上游服务类型列表 (只需要类型，节点从 Registry 发现)
    std::vector<std::string> upstream_services;
    
    // 性能配置
    int coalescer_interval_ms = 16;
    int replay_window_seconds = 300;
    
    // 安全配置
    std::string master_key_hex;
    
    // 日志配置
    std::string log_file;
    std::string log_level = "info";
};

} // namespace gateway
} // namespace gs
