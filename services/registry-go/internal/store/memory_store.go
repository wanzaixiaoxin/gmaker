package store

import (
	"context"
	"fmt"
	"sync"
	"time"

	pb "github.com/gmaker/game-server/gen/go/registry"
)

// MemoryStore 内存版 Registry Store（用于 Phase 1 无 Etcd 环境联调）
type MemoryStore struct {
	mu      sync.RWMutex
	nodes   map[string]*pb.NodeInfo       // node_id -> NodeInfo
	leases  map[string]int64              // node_id -> leaseID 计数器
	watches map[string][]chan *pb.NodeEvent // service_type -> event channels
	seqID   int64
	closed  bool
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		nodes:   make(map[string]*pb.NodeInfo),
		leases:  make(map[string]int64),
		watches: make(map[string][]chan *pb.NodeEvent),
		seqID:   1,
	}
}

func (m *MemoryStore) Register(ctx context.Context, node *pb.NodeInfo) (int64, error) {
	m.mu.Lock()
	leaseID := m.seqID
	m.seqID++
	m.nodes[node.NodeId] = node
	m.leases[node.NodeId] = leaseID

	// 复制 watchers 列表并在解锁后广播，避免同一线程内 Lock -> RLock 死锁
	var watchers []chan *pb.NodeEvent
	if arr, ok := m.watches[node.ServiceType]; ok {
		watchers = make([]chan *pb.NodeEvent, len(arr))
		copy(watchers, arr)
	}
	m.mu.Unlock()

	for _, ch := range watchers {
		select {
		case ch <- &pb.NodeEvent{Type: pb.NodeEvent_JOIN, Node: node}:
		default:
		}
	}
	return leaseID, nil
}

func (m *MemoryStore) Heartbeat(ctx context.Context, nodeID string) error {
	m.mu.RLock()
	_, ok := m.leases[nodeID]
	m.mu.RUnlock()
	if !ok {
		return fmt.Errorf("node not found: %s", nodeID)
	}
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
	m.mu.Lock()
	if m.closed {
		m.mu.Unlock()
		return nil, fmt.Errorf("store closed")
	}
	ch := make(chan *pb.NodeEvent, 16)
	m.watches[serviceType] = append(m.watches[serviceType], ch)
	m.mu.Unlock()

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

	return ch, nil
}

func (m *MemoryStore) broadcast(serviceType string, ev *pb.NodeEvent) {
	m.mu.RLock()
	arr := m.watches[serviceType]
	m.mu.RUnlock()
	for _, ch := range arr {
		select {
		case ch <- ev:
		default:
		}
	}
}

func (m *MemoryStore) Close() error {
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
