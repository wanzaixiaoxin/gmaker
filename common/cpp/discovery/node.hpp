#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace gs {
namespace discovery {

// NodeEventType 节点变更事件类型
enum class NodeEventType {
    Join   = 0,
    Leave  = 1,
    Update = 2,
};

// NodeInfo 服务节点信息
struct NodeInfo {
    std::string service_type;
    std::string node_id;
    std::string host;
    uint32_t    port = 0;
    std::map<std::string, std::string> metadata;
    uint64_t    load_score = 0;
    uint64_t    register_at = 0;
};

// NodeEvent 节点变更事件
struct NodeEvent {
    NodeEventType type;
    NodeInfo      node;
};

} // namespace discovery
} // namespace gs
