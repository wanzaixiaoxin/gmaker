#pragma once

#include "node.hpp"
#include <string>
#include <vector>
#include <functional>

namespace gs {
namespace discovery {

using EventCallback = std::function<void(const NodeEvent&)>;

// ServiceDiscovery 统一服务发现接口
// 对 C++ 服务隐藏底层是直连 Registry 还是直连 etcd 的差异。
class ServiceDiscovery {
public:
    virtual ~ServiceDiscovery() = default;

    // 注册当前服务节点，内部应自动维持心跳/租约
    virtual bool Register(const NodeInfo& node) = 0;

    // 注销当前服务节点
    virtual bool Deregister(const std::string& node_id) = 0;

    // 一次性发现某类服务的全部节点
    virtual bool Discover(const std::string& service_type, std::vector<NodeInfo>& out) = 0;

    // 持续监听多个服务类型的节点变更
    virtual bool Watch(const std::vector<std::string>& service_types, EventCallback callback) = 0;

    // 关闭连接，清理资源
    virtual void Close() = 0;
};

} // namespace discovery
} // namespace gs
