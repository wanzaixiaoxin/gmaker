#pragma once

#include "interface.hpp"
#include <memory>
#include <string>
#include <vector>

namespace gs {
namespace discovery {

// CreateDiscovery 根据类型创建 ServiceDiscovery 实例
// type: "registry" 或 "etcd"（若编译时启用了 etcd 支持）
// addrs: 地址列表，格式取决于 type：
//   - registry: ["host:port", ...]
//   - etcd:     ["http://host:port", ...]
std::unique_ptr<ServiceDiscovery> CreateDiscovery(
    const std::string& type,
    const std::vector<std::string>& addrs);

} // namespace discovery
} // namespace gs
