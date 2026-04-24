package discovery

import "context"

// NodeEventType 节点变更事件类型
type NodeEventType int

const (
	NodeEventJoin   NodeEventType = 0
	NodeEventLeave  NodeEventType = 1
	NodeEventUpdate NodeEventType = 2
)

// NodeInfo 服务节点信息
type NodeInfo struct {
	ServiceType string            `json:"service_type"`
	NodeID      string            `json:"node_id"`
	Host        string            `json:"host"`
	Port        uint32            `json:"port"`
	Metadata    map[string]string `json:"metadata,omitempty"`
	LoadScore   uint64            `json:"load_score"`
	RegisterAt  uint64            `json:"register_at"`
}

// NodeEvent 节点变更事件
type NodeEvent struct {
	Type NodeEventType
	Node NodeInfo
}

// ServiceDiscovery 统一服务发现接口
// 对业务服务隐藏底层是直连 Registry 还是直连 etcd 的差异。
type ServiceDiscovery interface {
	// Register 注册当前服务节点，内部应自动维持心跳/租约
	Register(ctx context.Context, node NodeInfo) error

	// Deregister 注销当前服务节点
	Deregister(ctx context.Context, nodeID string) error

	// Discover 一次性发现某类服务的全部节点
	Discover(ctx context.Context, serviceType string) ([]NodeInfo, error)

	// Watch 持续监听多个服务类型的节点变更
	// 回调会在后台 goroutine 中被调用，实现需保证线程安全。
	Watch(ctx context.Context, serviceTypes []string, callback func(NodeEvent)) error

	// Close 关闭连接，清理资源（停止心跳、释放租约等）
	Close() error
}
