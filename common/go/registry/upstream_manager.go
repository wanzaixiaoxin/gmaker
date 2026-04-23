package registry

import (
	"fmt"
	"sync"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/registry"
)

// UpstreamManager 上游服务管理器
// 封装了向 Registry 批量订阅、初始化连接池、动态增删节点的完整逻辑。
// 使用方式：
//   1. NewUpstreamManager(client)
//   2. 为每种关注的服务调用 AddInterest
//   3. 调用 Start() 启动订阅与连接池
//   4. 通过 GetPool(serviceType) 获取连接池发送数据
//   5. 服务退出时调用 Stop()
type UpstreamManager struct {
	client    *Client
	pools     map[string]*net.UpstreamPool // service_type -> pool
	interests map[string]func(*net.TCPConn, *net.Packet)
	mu        sync.RWMutex
}

// NewUpstreamManager 创建上游管理器
func NewUpstreamManager(client *Client) *UpstreamManager {
	return &UpstreamManager{
		client:    client,
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

// Start 向 Registry 批量订阅所有关注的服务类型，
// 收到全量快照后初始化各连接池节点，并启动连接池。
// 后续节点上下线通过 NodeEvent 自动增删。
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

	res, err := m.client.Subscribe(types, m.onNodeEvent)
	if err != nil {
		return fmt.Errorf("subscribe failed: %w", err)
	}

	// 根据全量快照初始化各 pool 的节点
	m.mu.Lock()
	for svcType, list := range res.Snapshot {
		pool, ok := m.pools[svcType]
		if !ok {
			continue
		}
		for _, node := range list.Nodes {
			addr := fmt.Sprintf("%s:%d", node.Host, node.Port)
			pool.AddNode(addr)
		}
	}
	// 启动所有连接池
	for _, pool := range m.pools {
		pool.Start()
	}
	m.mu.Unlock()

	return nil
}

// Stop 停止所有上游连接池
func (m *UpstreamManager) Stop() {
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, pool := range m.pools {
		pool.Stop()
	}
}

// GetPool 获取指定服务类型的连接池
func (m *UpstreamManager) GetPool(serviceType string) *net.UpstreamPool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.pools[serviceType]
}

// onNodeEvent 处理 Registry 推送的增量事件
func (m *UpstreamManager) onNodeEvent(ev *pb.NodeEvent) {
	if ev == nil || ev.Node == nil {
		return
	}
	node := ev.Node
	svcType := node.ServiceType
	addr := fmt.Sprintf("%s:%d", node.Host, node.Port)

	m.mu.RLock()
	pool, ok := m.pools[svcType]
	m.mu.RUnlock()
	if !ok {
		return
	}

	switch ev.Type {
	case pb.NodeEvent_JOIN, pb.NodeEvent_UPDATE:
		pool.AddNode(addr)
	case pb.NodeEvent_LEAVE:
		pool.RemoveNode(addr)
	}
}
