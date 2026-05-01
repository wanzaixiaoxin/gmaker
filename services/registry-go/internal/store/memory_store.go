package store

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	pb "github.com/gmaker/luffa/gen/go/registry"
)

// MemoryStore 内存版 Registry Store（用于 Phase 1 无 Etcd 环境联调）
type MemoryStore struct {
	mu            sync.RWMutex
	nodes         map[string]*pb.NodeInfo
	leases        map[string]int64
	lastHeartbeat map[string]time.Time
	watches       map[string][]chan *pb.NodeEvent
	seqID         int64
	closed        bool
	stopCh        chan struct{}
	wg            sync.WaitGroup
}

const defaultTTL = 30 * time.Second

func NewMemoryStore() *MemoryStore {
	m := &MemoryStore{
		nodes:         make(map[string]*pb.NodeInfo),
		leases:        make(map[string]int64),
		lastHeartbeat: make(map[string]time.Time),
		watches:       make(map[string][]chan *pb.NodeEvent),
		seqID:         1,
		stopCh:        make(chan struct{}),
	}
	m.wg.Add(1)
	go m.ttlSweepLoop()
	return m
}

func (m *MemoryStore) ttlSweepLoop() {
	defer m.wg.Done()
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			m.sweepExpired()
		case <-m.stopCh:
			return
		}
	}
}

func (m *MemoryStore) sweepExpired() {
	now := time.Now()
	var expired []string
	m.mu.Lock()
	for nodeID, last := range m.lastHeartbeat {
		if now.Sub(last) > defaultTTL {
			expired = append(expired, nodeID)
		}
	}
	for _, nodeID := range expired {
		node := m.nodes[nodeID]
		if node != nil {
			m.broadcast(node.ServiceType, &pb.NodeEvent{Type: pb.NodeEvent_LEAVE, Node: node})
		}
		delete(m.nodes, nodeID)
		delete(m.leases, nodeID)
		delete(m.lastHeartbeat, nodeID)
		logger.Infof("[MemoryStore] TTL expired: node=%s", nodeID)
	}
	m.mu.Unlock()
}

func (m *MemoryStore) Register(ctx context.Context, node *pb.NodeInfo) (int64, error) {
	m.mu.Lock()
	leaseID := m.seqID
	m.seqID++
	m.nodes[node.NodeId] = node
	m.leases[node.NodeId] = leaseID
	m.lastHeartbeat[node.NodeId] = time.Now()

	// 复制 watchers 列表并在解锁后广播，避免同一线程内 Lock -> RLock 死锁
	var watchers []chan *pb.NodeEvent
	if arr, ok := m.watches[node.ServiceType]; ok {
		watchers = make([]chan *pb.NodeEvent, len(arr))
		copy(watchers, arr)
	}
	m.mu.Unlock()

	logger.Infof("[MemoryStore] Node JOIN: %s/%s @ %s:%d, lease=%d, watchers=%d",
		node.ServiceType, node.NodeId, node.Host, node.Port, leaseID, len(watchers))

	for _, ch := range watchers {
		select {
		case ch <- &pb.NodeEvent{Type: pb.NodeEvent_JOIN, Node: node}:
		default:
		}
	}
	return leaseID, nil
}

func (m *MemoryStore) Heartbeat(ctx context.Context, nodeID string) error {
	m.mu.Lock()
	_, ok := m.leases[nodeID]
	if ok {
		m.lastHeartbeat[nodeID] = time.Now()
	}
	m.mu.Unlock()
	if !ok {
		logger.Warnf("[MemoryStore] Heartbeat failed: node=%s not found", nodeID)
		return fmt.Errorf("node not found: %s", nodeID)
	}
	logger.Debugf("[MemoryStore] Heartbeat ok: node=%s", nodeID)
	return nil
}

func (m *MemoryStore) Discover(ctx context.Context, serviceType string) ([]*pb.NodeInfo, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	var out []*pb.NodeInfo
	for _, n := range m.nodes {
		if n.ServiceType == serviceType {
			out = append(out, n)
		}
	}
	return out, nil
}

func (m *MemoryStore) Watch(ctx context.Context, serviceType string) (<-chan *pb.NodeEvent, error) {
	ch, _, err := m.subscribeInternal(ctx, serviceType, true)
	return ch, err
}

func (m *MemoryStore) Subscribe(ctx context.Context, serviceType string) (<-chan *pb.NodeEvent, []*pb.NodeInfo, error) {
	return m.subscribeInternal(ctx, serviceType, false)
}

func (m *MemoryStore) subscribeInternal(ctx context.Context, serviceType string, sendSnapshot bool) (chan *pb.NodeEvent, []*pb.NodeInfo, error) {
	m.mu.Lock()
	if m.closed {
		m.mu.Unlock()
		return nil, nil, fmt.Errorf("store closed")
	}
	ch := make(chan *pb.NodeEvent, 16)
	m.watches[serviceType] = append(m.watches[serviceType], ch)

	// 收集当前全量节点
	var snapshot []*pb.NodeInfo
	for _, n := range m.nodes {
		if n.ServiceType == serviceType {
			snapshot = append(snapshot, n)
		}
	}
	m.mu.Unlock()

	if sendSnapshot {
		// 发送当前已有的节点作为初始快照
		go func() {
			time.Sleep(100 * time.Millisecond)
			m.mu.RLock()
			if m.closed {
				m.mu.RUnlock()
				return
			}
			for _, n := range m.nodes {
				if n.ServiceType == serviceType {
					select {
					case ch <- &pb.NodeEvent{Type: pb.NodeEvent_JOIN, Node: n}:
					default:
					}
				}
			}
			m.mu.RUnlock()
		}()
	}

	// 清理 goroutine
	go func() {
		<-ctx.Done()
		m.mu.Lock()
		if m.closed {
			m.mu.Unlock()
			return
		}
		arr := m.watches[serviceType]
		for i, c := range arr {
			if c == ch {
				m.watches[serviceType] = append(arr[:i], arr[i+1:]...)
				break
			}
		}
		close(ch)
		m.mu.Unlock()
	}()

	return ch, snapshot, nil
}

func (m *MemoryStore) broadcast(serviceType string, ev *pb.NodeEvent) {
	m.mu.RLock()
	arr := m.watches[serviceType]
	m.mu.RUnlock()
	if len(arr) > 0 && ev.Node != nil {
		logger.Infof("[MemoryStore] Broadcast %s to %d watchers: %s/%s",
			serviceType, len(arr), ev.Node.ServiceType, ev.Node.NodeId)
	}
	for _, ch := range arr {
		select {
		case ch <- ev:
		default:
		}
	}
}

func (m *MemoryStore) Close() error {
	close(m.stopCh)
	m.wg.Wait()

	m.mu.Lock()
	defer m.mu.Unlock()
	if m.closed {
		return nil
	}
	m.closed = true
	for _, arr := range m.watches {
		for _, ch := range arr {
			close(ch)
		}
	}
	m.watches = nil
	return nil
}
