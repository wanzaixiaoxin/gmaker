package store

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/gen/go/registry"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/proto"
)

const (
	DefaultTTL     = 15
	KeyPrefix      = "/registry/nodes/"
)

type EtcdStore struct {
	client   *clientv3.Client
	leases   sync.Map // node_id -> leaseID
}

func NewEtcdStore(endpoints string) (*EtcdStore, error) {
	eps := strings.Split(endpoints, ",")
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   eps,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, err
	}
	return &EtcdStore{client: cli}, nil
}

func (e *EtcdStore) Close() error {
	return e.client.Close()
}

func nodeKey(node *registry.NodeInfo) string {
	return fmt.Sprintf("%s%s/%s", KeyPrefix, node.ServiceType, node.NodeId)
}

func prefixKey(serviceType string) string {
	return fmt.Sprintf("%s%s/", KeyPrefix, serviceType)
}

func (e *EtcdStore) Register(ctx context.Context, node *registry.NodeInfo) (int64, error) {
	resp, err := e.client.Grant(ctx, DefaultTTL)
	if err != nil {
		logger.Errorf("[EtcdStore] Grant lease failed: %s/%s, err=%v", node.ServiceType, node.NodeId, err)
		return 0, err
	}

	data, err := proto.Marshal(node)
	if err != nil {
		return 0, err
	}

	key := nodeKey(node)
	_, err = e.client.Put(ctx, key, string(data), clientv3.WithLease(resp.ID))
	if err != nil {
		logger.Errorf("[EtcdStore] Put node failed: %s/%s, err=%v", node.ServiceType, node.NodeId, err)
		return 0, err
	}

	e.leases.Store(node.NodeId, resp.ID)
	logger.Infof("[EtcdStore] Node registered: %s/%s @ %s:%d, lease=%d", node.ServiceType, node.NodeId, node.Host, node.Port, resp.ID)
	return int64(resp.ID), nil
}

func (e *EtcdStore) Heartbeat(ctx context.Context, nodeID string) error {
	v, ok := e.leases.Load(nodeID)
	if !ok {
		logger.Warnf("[EtcdStore] Heartbeat failed: lease not found for node=%s", nodeID)
		return fmt.Errorf("lease not found for node: %s", nodeID)
	}
	leaseID := v.(clientv3.LeaseID)
	_, err := e.client.KeepAliveOnce(ctx, leaseID)
	if err != nil {
		logger.Warnf("[EtcdStore] Heartbeat failed: node=%s, lease=%d, err=%v", nodeID, leaseID, err)
		return err
	}
	logger.Debugf("[EtcdStore] Heartbeat ok: node=%s, lease=%d", nodeID, leaseID)
	return nil
}

func (e *EtcdStore) Discover(ctx context.Context, serviceType string) ([]*registry.NodeInfo, error) {
	resp, err := e.client.Get(ctx, prefixKey(serviceType), clientv3.WithPrefix())
	if err != nil {
		return nil, err
	}

	nodes := make([]*registry.NodeInfo, 0, len(resp.Kvs))
	for _, kv := range resp.Kvs {
		var node registry.NodeInfo
		if err := proto.Unmarshal(kv.Value, &node); err != nil {
			continue
		}
		nodes = append(nodes, &node)
	}
	return nodes, nil
}

func (e *EtcdStore) Watch(ctx context.Context, serviceType string) (<-chan *registry.NodeEvent, error) {
	out := make(chan *registry.NodeEvent, 16)
	watchCh := e.client.Watch(ctx, prefixKey(serviceType), clientv3.WithPrefix())

	go func() {
		defer close(out)
		for watchResp := range watchCh {
			if watchResp.Err() != nil {
				return
			}
			for _, ev := range watchResp.Events {
				var node registry.NodeInfo
				if err := proto.Unmarshal(ev.Kv.Value, &node); err != nil {
					continue
				}
				var tp registry.NodeEvent_Type
				switch ev.Type {
				case clientv3.EventTypePut:
					// 如果 key 之前不存在则为 JOIN，否则为 UPDATE
					tp = registry.NodeEvent_JOIN
				case clientv3.EventTypeDelete:
					tp = registry.NodeEvent_LEAVE
				}
				select {
				case out <- &registry.NodeEvent{Type: tp, Node: &node}:
				case <-ctx.Done():
					return
				}
			}
		}
	}()

	return out, nil
}

func (e *EtcdStore) Subscribe(ctx context.Context, serviceType string) (<-chan *registry.NodeEvent, []*registry.NodeInfo, error) {
	// 获取当前全量
	nodes, err := e.Discover(ctx, serviceType)
	if err != nil {
		logger.Errorf("[EtcdStore] Discover failed for %s: %v", serviceType, err)
		return nil, nil, err
	}
	logger.Infof("[EtcdStore] Subscribe snapshot: %s has %d nodes", serviceType, len(nodes))

	// 创建事件通道并启动 Watch（仅增量，不重复发送全量）
	out := make(chan *registry.NodeEvent, 16)
	watchCh := e.client.Watch(ctx, prefixKey(serviceType), clientv3.WithPrefix())

	go func() {
		defer close(out)
		for watchResp := range watchCh {
			if watchResp.Err() != nil {
				logger.Errorf("[EtcdStore] Watch error: %s, err=%v", serviceType, watchResp.Err())
				return
			}
			for _, ev := range watchResp.Events {
				var node registry.NodeInfo
				if err := proto.Unmarshal(ev.Kv.Value, &node); err != nil {
					continue
				}
				var tp registry.NodeEvent_Type
				switch ev.Type {
				case clientv3.EventTypePut:
					tp = registry.NodeEvent_JOIN
					logger.Infof("[EtcdStore] Node JOIN: %s/%s", node.ServiceType, node.NodeId)
				case clientv3.EventTypeDelete:
					tp = registry.NodeEvent_LEAVE
					logger.Infof("[EtcdStore] Node LEAVE: %s/%s", node.ServiceType, node.NodeId)
				}
				select {
				case out <- &registry.NodeEvent{Type: tp, Node: &node}:
				case <-ctx.Done():
					return
				}
			}
		}
	}()

	return out, nodes, nil
}
