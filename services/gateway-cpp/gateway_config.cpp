#include "gateway_config.hpp"
#include "gs/config/json_parser.hpp"
#include <iostream>

namespace gs {
namespace gateway {

Config LoadConfig(const std::string& path) {
    Config cfg;
    auto json = config::LoadJson(path);
    
    auto service = json.GetObject("service");
    cfg.service_type = service.GetString("service_type", "gateway");
    cfg.node_id = service.GetString("node_id", "gateway-1");
    cfg.log_level = service.GetString("log_level", "info");
    cfg.log_file = service.GetString("log_file", "");
    cfg.metrics_addr = service.GetString("metrics_addr", ":9081");
    
    auto network = json.GetObject("network");
    cfg.listen_port = static_cast<uint16_t>(network.GetInt("port", 8081));
    cfg.max_connections = static_cast<int>(network.GetInt("max_connections", 10000));
    
    auto registry = json.GetObject("registry");
    for (const auto& node : registry.GetNodeArray("nodes")) {
        cfg.registry_nodes.push_back(node.host + ":" + std::to_string(node.port));
    }
    
    auto upstream = json.GetObject("upstream");
    cfg.upstream_services = upstream.GetStringArray("services");
    
    cfg.coalescer_interval_ms = static_cast<int>(json.GetInt("coalescer_interval_ms", 16));
    
    auto security = json.GetObject("security");
    cfg.master_key_hex = security.GetString("master_key_hex", "");
    cfg.replay_window_seconds = static_cast<int>(security.GetInt("replay_window_seconds", 300));
    
    return cfg;
}

void PrintConfig(const Config& cfg) {
    std::cout << "=== Gateway Configuration ===" << std::endl;
    std::cout << "Service Type:     " << cfg.service_type << std::endl;
    std::cout << "Node ID:          " << cfg.node_id << std::endl;
    std::cout << "Listen port:      " << cfg.listen_port << std::endl;
    std::cout << "Max connections:  " << cfg.max_connections << std::endl;
    std::cout << "Metrics addr:     " << cfg.metrics_addr << std::endl;
    std::cout << "Log level:        " << cfg.log_level << std::endl;
    std::cout << "Registry nodes:   ";
    for (const auto& addr : cfg.registry_nodes) std::cout << addr << " ";
    std::cout << std::endl;
    std::cout << "Upstream services:";
    for (const auto& svc : cfg.upstream_services) std::cout << " " << svc;
    std::cout << std::endl;
    std::cout << "=============================" << std::endl;
}

std::vector<uint8_t> ParseMasterKey(const Config& cfg) {
    std::vector<uint8_t> key(32, 0);
    if (cfg.master_key_hex.empty()) return key;
    
    std::string hex = cfg.master_key_hex;
    if (hex.size() >= 2 && (hex.substr(0, 2) == "0x" || hex.substr(0, 2) == "0X")) {
        hex = hex.substr(2);
    }
    
    key.clear();
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        key.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return key;
}

} // namespace gateway
} // namespace gs
