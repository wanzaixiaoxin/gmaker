package discovery

import (
	"context"
	"encoding/json"
	"fmt"
	"path"
	"strings"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// EtcdImpl 基于 etcd v3 的 ServiceDiscovery 实现
type EtcdImpl struct {
	client         *clientv3.Client
	leaseID        clientv3.LeaseID
	kaCancel       context.CancelFunc
	nodeID         string
	mu             sync.Mutex
	watchesCancel  context.CancelFunc
}

// NewEtcdImpl 创建 etcd 模式的服务发现实例
func NewEtcdImpl(addrs []string) (*EtcdImpl, error) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   addrs,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, fmt.Errorf("connect etcd failed: %w", err)
	}
	return &EtcdImpl{client: cli}, nil
}

// Register 注册节点并启动租约自动续期
func (e *EtcdImpl) Register(ctx context.Context, node NodeInfo) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	e.nodeID = node.NodeID

	// 创建 10 秒 TTL 租约
	leaseResp, err := e.client.Grant(ctx, 10)
	if err != nil {
		return fmt.Errorf("etcd grant lease failed: %w", err)
	}
	e.leaseID = leaseResp.ID

	// 启动 KeepAlive（自动在后台续期）
	kaCtx, cancel := context.WithCancel(context.Background())
	e.kaCancel = cancel
	kaCh, err := e.client.KeepAlive(kaCtx, e.leaseID)
	if err != nil {
		return fmt.Errorf("etcd keepalive failed: %w", err)
	}

	// 写节点信息到 /services/{service_type}/{node_id}
	key := fmt.Sprintf("/services/%s/%s", node.ServiceType, node.NodeID)
	val, _ := json.Marshal(node)
	_, err = e.client.Put(ctx, key, string(val), clientv3.WithLease(e.leaseID))
	if err != nil {
		return fmt.Errorf("etcd put failed: %w", err)
	}

	// 后台消费 keepalive 响应通道，防止阻塞
	go func() {
		for range kaCh {
			// keepalive 正常响应，无需处理
		}
		logger.Infof("etcd keepalive channel closed for node %s", e.nodeID)
	}()

	return nil
}

// Deregister 注销节点，撤销租约（自动删除该租约下的所有 key）
func (e *EtcdImpl) Deregister(ctx context.Context, nodeID string) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	if e.kaCancel != nil {
		e.kaCancel()
		e.kaCancel = nil
	}

	if e.leaseID != 0 {
		_, err := e.client.Revoke(ctx, e.leaseID)
		if err != nil {
			logger.Warnf("etcd revoke lease failed: %v", err)
		}
		e.leaseID = 0
	}
	return nil
}

// Discover 发现某类服务的全部节点
func (e *EtcdImpl) Discover(ctx context.Context, serviceType string) ([]NodeInfo, error) {
	key := fmt.Sprintf("/services/%s", serviceType)
	resp, err := e.client.Get(ctx, key, clientv3.WithPrefix())
	if err != nil {
		return nil, fmt.Errorf("etcd get failed: %w", err)
	}

	var nodes []NodeInfo
	for _, kv := range resp.Kvs {
		var node NodeInfo
		if err := json.Unmarshal(kv.Value, &node); err == nil {
			nodes = append(nodes, node)
		}
	}
	return nodes, nil
}

// Watch 持续监听多个服务类型的节点变更
func (e *EtcdImpl) Watch(ctx context.Context, serviceTypes []string, callback func(NodeEvent)) error {
	watchCtx, cancel := context.WithCancel(ctx)
	e.mu.Lock()
	e.watchesCancel = cancel
	e.mu.Unlock()

	var wg sync.WaitGroup
	for _, svc := range serviceTypes {
		wg.Add(1)
		go func(serviceType string) {
			defer wg.Done()
			key := fmt.Sprintf("/services/%s", serviceType)
			wch := e.client.Watch(watchCtx, key, clientv3.WithPrefix())
			for wresp := range wch {
				for _, ev := range wresp.Events {
					var node NodeInfo
					var eventType NodeEventType

					if ev.Type == clientv3.EventTypePut {
						// 解析节点信息
						if err := json.Unmarshal(ev.Kv.Value, &node); err != nil {
							logger.Warnf("etcd watch unmarshal failed: %v", err)
							continue
						}
						// etcd 不区分 create vs update，统一视为 UPDATE
						// 业务层可通过 LoadScore 等字段判断是否有实质变化
						eventType = NodeEventUpdate
					} else if ev.Type == clientv3.EventTypeDelete {
						eventType = NodeEventLeave
						// 从 key 中提取 service_type 和 node_id
						parts := strings.Split(path.Clean(string(ev.Kv.Key)), "/")
						if len(parts) >= 4 {
							node.ServiceType = parts[2]
							node.NodeID = parts[3]
						}
					}

					callback(NodeEvent{
						Type: eventType,
						Node: node,
					})
				}
			}
		}(svc)
	}

	wg.Wait()
	return nil
}

// Close 关闭 etcd 连接
func (e *EtcdImpl) Close() error {
	e.mu.Lock()
	if e.kaCancel != nil {
		e.kaCancel()
		e.kaCancel = nil
	}
	if e.watchesCancel != nil {
		e.watchesCancel()
		e.watchesCancel = nil
	}
	e.mu.Unlock()

	return e.client.Close()
}
