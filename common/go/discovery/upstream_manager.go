package discovery

import (
	"context"
	"fmt"
	"sync"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
)

// UpstreamManager 上游服务管理器
// 与具体服务发现后端（Registry 或 etcd）解耦，通过 ServiceDiscovery 接口操作。
type UpstreamManager struct {
	sd        ServiceDiscovery
	pools     map[string]*net.UpstreamPool // service_type -> pool
	interests map[string]func(*net.TCPConn, *net.Packet)
	mu        sync.RWMutex
}

// NewUpstreamManager 创建上游管理器
func NewUpstreamManager(sd ServiceDiscovery) *UpstreamManager {
	return &UpstreamManager{
		sd:        sd,
		pools:     make(map[string]*net.UpstreamPool),
		interests: make(map[string]func(*net.TCPConn, *net.Packet)),
	}
}

// AddInterest 声明对某类上游服务的兴趣
// onData: 收到该上游回包时的回调（可为 nil，若只发不收）
// 返回对应的连接池（尚未启动，Start 后才会真正连接）
func (m *UpstreamManager) AddInterest(serviceType string, onData func(*net.TCPConn, *net.Packet)) *net.UpstreamPool {
	m.mu.Lock()
	defer m.mu.Unlock()

	pool := net.NewUpstreamPool(onData)
	m.pools[serviceType] = pool
	m.interests[serviceType] = onData
	return pool
}

// Start 向 ServiceDiscovery 批量订阅所有关注的服务类型，
// 通过 Discover 获取全量快照初始化各连接池节点，并启动连接池。
// 后续节点上下线通过 Watch 自动增删。
func (m *UpstreamManager) Start() error {
	m.mu.RLock()
	var types []string
	for t := range m.interests {
		types = append(types, t)
	}
	m.mu.RUnlock()

	if len(types) == 0 {
		return nil
	}

	logger.Infof("[UpstreamManager] Subscribing to services: %v", types)

	// 先通过 Discover 获取全量快照初始化节点
	for _, svc := range types {
		nodes, err := m.sd.Discover(context.Background(), svc)
		if err != nil {
			logger.Warnf("[UpstreamManager] Discover %s failed: %v", svc, err)
			continue
		}
		m.mu.Lock()
		pool, ok := m.pools[svc]
		if ok {
			for _, node := range nodes {
				addr := fmt.Sprintf("%s:%d", node.Host, node.Port)
				pool.AddNode(addr)
				logger.Infof("[UpstreamManager] Snapshot add node: %s/%s @ %s", svc, node.NodeID, addr)
			}
		}
		m.mu.Unlock()
	}

	// 启动所有连接池
	m.mu.Lock()
	for svcType, pool := range m.pools {
		pool.Start()
		logger.Infof("[UpstreamManager] Pool started: %s (healthy=%d/%d)", svcType, pool.HealthyCount(), pool.TotalCount())
	}
	m.mu.Unlock()

	// 后台启动 Watch 监听增量变更
	go func() {
		ctx := context.Background()
		if err := m.sd.Watch(ctx, types, m.onNodeEvent); err != nil {
			logger.Warnf("[UpstreamManager] Watch failed: %v", err)
		}
	}()

	return nil
}

// Stop 停止所有上游连接池
func (m *UpstreamManager) Stop() {
	m.mu.Lock()
	defer m.mu.Unlock()
	for svcType, pool := range m.pools {
		pool.Stop()
		logger.Infof("[UpstreamManager] Pool stopped: %s", svcType)
	}
}

// GetPool 获取指定服务类型的连接池
func (m *UpstreamManager) GetPool(serviceType string) *net.UpstreamPool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.pools[serviceType]
}

// onNodeEvent 处理 ServiceDiscovery 推送的增量事件
func (m *UpstreamManager) onNodeEvent(ev NodeEvent) {
	if ev.Node.Host == "" {
		return
	}
	svcType := ev.Node.ServiceType
	addr := fmt.Sprintf("%s:%d", ev.Node.Host, ev.Node.Port)

	m.mu.RLock()
	pool, ok := m.pools[svcType]
	m.mu.RUnlock()
	if !ok {
		return
	}

	switch ev.Type {
	case NodeEventJoin, NodeEventUpdate:
		logger.Infof("[UpstreamManager] Node JOIN/UPDATE: %s/%s @ %s", svcType, ev.Node.NodeID, addr)
		pool.AddNode(addr)
	case NodeEventLeave:
		logger.Infof("[UpstreamManager] Node LEAVE: %s/%s @ %s", svcType, ev.Node.NodeID, addr)
		pool.RemoveNode(addr)
	}
}
