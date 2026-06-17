package discovery

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
)

// UpstreamManager 上游服务管理器
// 与具体服务发现后端（Registry 或 etcd）解耦，通过 ServiceDiscovery 接口操作。
type UpstreamManager struct {
	sd        ServiceDiscovery
	pools     map[string]*net.UpstreamPool // service_type -> pool
	interests map[string]func(*net.TCPConn, *net.Packet)
	// nodeAddrs 记录 nodeID -> addr 的映射。
	// 当 etcd Delete 事件携带的 Host 为空时，据此回退定位要移除的节点地址，
	// 避免死节点残留在连接池中（审计 P0 #6）。
	nodeAddrs map[string]string
	mu        sync.RWMutex
}

// NewUpstreamManager 创建上游管理器
func NewUpstreamManager(sd ServiceDiscovery) *UpstreamManager {
	return &UpstreamManager{
		sd:        sd,
		pools:     make(map[string]*net.UpstreamPool),
		interests: make(map[string]func(*net.TCPConn, *net.Packet)),
		nodeAddrs: make(map[string]string),
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
				m.nodeAddrs[svc+"/"+node.NodeID] = addr
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
		backoff := time.Second
		maxBackoff := 30 * time.Second
		for {
			if err := m.sd.Watch(ctx, types, m.onNodeEvent); err != nil {
				logger.Warnf("[UpstreamManager] Watch failed: %v, retrying in %v", err, backoff)
			}
			select {
			case <-time.After(backoff):
				backoff = backoff * 2
				if backoff > maxBackoff {
					backoff = maxBackoff
				}
			}
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

// onNodeEvent 处理 ServiceDiscovery 推送的增量事件。
// 注意：etcd 的 Delete 事件通常不携带 value，Host/Port 为零值。
// 此时 LEAVE 事件需通过 nodeID 回退查 addr，否则死节点会残留在连接池中（审计 P0 #6）。
func (m *UpstreamManager) onNodeEvent(ev NodeEvent) {
	svcType := ev.Node.ServiceType
	nodeKey := svcType + "/" + ev.Node.NodeID

	// Host 为空时：JOIN/UPDATE 无法处理（没有地址），但 LEAVE 可回退定位。
	if ev.Node.Host == "" {
		if ev.Type != NodeEventLeave {
			return
		}
		m.mu.Lock()
		addr, ok := m.nodeAddrs[nodeKey]
		m.mu.Unlock()
		if !ok {
			// 未记录过该节点地址，无法移除（可能是 Watch 启动前已存在的历史节点）
			return
		}
		m.mu.RLock()
		pool, ok := m.pools[svcType]
		m.mu.RUnlock()
		if !ok {
			return
		}
		logger.Infof("[UpstreamManager] Node LEAVE (by NodeID): %s @ %s", nodeKey, addr)
		pool.RemoveNode(addr)
		m.mu.Lock()
		delete(m.nodeAddrs, nodeKey)
		m.mu.Unlock()
		return
	}

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
		// 记录 nodeID -> addr 映射，供后续 Host 为空的 LEAVE 事件回退定位
		m.mu.Lock()
		m.nodeAddrs[nodeKey] = addr
		m.mu.Unlock()
	case NodeEventLeave:
		logger.Infof("[UpstreamManager] Node LEAVE: %s/%s @ %s", svcType, ev.Node.NodeID, addr)
		pool.RemoveNode(addr)
		m.mu.Lock()
		delete(m.nodeAddrs, nodeKey)
		m.mu.Unlock()
	}
}
