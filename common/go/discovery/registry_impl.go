package discovery

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/registry"
	pb "github.com/gmaker/luffa/gen/go/registry"
)

// RegistryImpl 基于自研 Registry 的 ServiceDiscovery 实现
type RegistryImpl struct {
	client        *registry.Client
	heartbeatStop chan struct{}
	heartbeatWG   sync.WaitGroup
	nodeID        string
}

// NewRegistryImpl 创建 Registry 模式的服务发现实例
func NewRegistryImpl(addrs []string) *RegistryImpl {
	return &RegistryImpl{
		client:        registry.NewClient(addrs),
		heartbeatStop: make(chan struct{}),
	}
}

// Register 注册节点并启动后台心跳保活
func (r *RegistryImpl) Register(ctx context.Context, node NodeInfo) error {
	if err := r.client.Connect(); err != nil {
		return fmt.Errorf("connect registry failed: %w", err)
	}

	pbNode := &pb.NodeInfo{
		ServiceType: node.ServiceType,
		NodeId:      node.NodeID,
		Host:        node.Host,
		Port:        node.Port,
		Metadata:    node.Metadata,
		LoadScore:   node.LoadScore,
		RegisterAt:  node.RegisterAt,
	}

	r.nodeID = node.NodeID
	if _, err := r.client.RegisterWithRetry(pbNode, 5); err != nil {
		return fmt.Errorf("register failed: %w", err)
	}

	// 启动心跳 goroutine
	r.heartbeatWG.Add(1)
	go func() {
		defer r.heartbeatWG.Done()
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				_, err := r.client.Heartbeat(r.nodeID)
				if err != nil {
					logger.Warnf("registry heartbeat failed: %v", err)
				}
			case <-r.heartbeatStop:
				return
			}
		}
	}()

	return nil
}

// Deregister 注销节点并停止心跳
func (r *RegistryImpl) Deregister(ctx context.Context, nodeID string) error {
	close(r.heartbeatStop)
	r.heartbeatWG.Wait()
	// 自研 Registry 目前无显式注销接口，依赖心跳超时自动清理
	return nil
}

// Discover 发现服务节点
func (r *RegistryImpl) Discover(ctx context.Context, serviceType string) ([]NodeInfo, error) {
	list, err := r.client.Discover(serviceType)
	if err != nil {
		return nil, err
	}

	var nodes []NodeInfo
	for _, n := range list.Nodes {
		nodes = append(nodes, pbNodeToNodeInfo(n))
	}
	return nodes, nil
}

// Watch 监听节点变更
func (r *RegistryImpl) Watch(ctx context.Context, serviceTypes []string, callback func(NodeEvent)) error {
	_, err := r.client.Subscribe(serviceTypes, func(ev *pb.NodeEvent) {
		callback(pbEventToNodeEvent(ev))
	})
	return err
}

// Close 关闭 Registry 连接
func (r *RegistryImpl) Close() error {
	select {
	case <-r.heartbeatStop:
		// 已关闭
	default:
		close(r.heartbeatStop)
	}
	r.heartbeatWG.Wait()
	r.client.Close()
	return nil
}

func pbNodeToNodeInfo(n *pb.NodeInfo) NodeInfo {
	return NodeInfo{
		ServiceType: n.ServiceType,
		NodeID:      n.NodeId,
		Host:        n.Host,
		Port:        n.Port,
		Metadata:    n.Metadata,
		LoadScore:   n.LoadScore,
		RegisterAt:  n.RegisterAt,
	}
}

func pbEventToNodeEvent(ev *pb.NodeEvent) NodeEvent {
	var t NodeEventType
	switch ev.Type {
	case pb.NodeEvent_JOIN:
		t = NodeEventJoin
	case pb.NodeEvent_LEAVE:
		t = NodeEventLeave
	case pb.NodeEvent_UPDATE:
		t = NodeEventUpdate
	}
	return NodeEvent{
		Type: t,
		Node: pbNodeToNodeInfo(ev.Node),
	}
}
