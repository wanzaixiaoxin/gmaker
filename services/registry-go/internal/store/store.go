package store

import (
	"context"

	pb "github.com/gmaker/luffa/gen/go/registry"
)

// Store 定义 Registry 底层存储接口
type Store interface {
	// Register 注册节点，返回 LeaseID
	Register(ctx context.Context, node *pb.NodeInfo) (int64, error)

	// Heartbeat 续租节点
	Heartbeat(ctx context.Context, nodeID string) error

	// Discover 发现指定类型的节点列表
	Discover(ctx context.Context, serviceType string) ([]*pb.NodeInfo, error)

	// Watch 监听指定类型的节点变更（含初始快照推送）
	Watch(ctx context.Context, serviceType string) (<-chan *pb.NodeEvent, error)

	// Subscribe 订阅指定类型的节点变更（不含初始快照，返回当前全量列表）
	Subscribe(ctx context.Context, serviceType string) (<-chan *pb.NodeEvent, []*pb.NodeInfo, error)

	// Close 关闭存储连接
	Close() error
}
