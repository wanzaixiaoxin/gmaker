package net

import (
	"sync"
	"sync/atomic"
	"time"
)

// Node 上游节点运行时状态
type Node struct {
	Addr      string
	Client    *TCPClient
	Healthy   atomic.Bool
	Connecting atomic.Bool
	LastActive time.Time
}

// UpstreamPool 上游连接池：多节点、轮询、自动重连
type UpstreamPool struct {
	mu       sync.RWMutex
	nodes    []*Node
	rrIndex  atomic.Uint64

	onData   func(*TCPConn, *Packet)
	onNodeEvent func(addr string, healthy bool) // 节点健康状态变更回调

	running  atomic.Bool
	stopCh   chan struct{}
	wg       sync.WaitGroup
}

// NewUpstreamPool 创建连接池
func NewUpstreamPool(onData func(*TCPConn, *Packet)) *UpstreamPool {
	return &UpstreamPool{
		onData: onData,
		stopCh: make(chan struct{}),
	}
}

// SetOnNodeEvent 设置节点健康状态变更回调
func (p *UpstreamPool) SetOnNodeEvent(fn func(addr string, healthy bool)) {
	p.onNodeEvent = fn
}

// AddNode 添加上游节点（Start 前调用）
func (p *UpstreamPool) AddNode(addr string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	for _, n := range p.nodes {
		if n.Addr == addr {
			return // 已存在
		}
	}
	n := &Node{Addr: addr}
	n.Healthy.Store(false)
	n.Connecting.Store(false)
	p.nodes = append(p.nodes, n)
}

// SetOnData 动态设置收到数据时的回调（用于 Registry 发现模式后绑定 rpc.Client）
func (p *UpstreamPool) SetOnData(onData func(*TCPConn, *Packet)) {
	p.mu.Lock()
	p.onData = onData
	p.mu.Unlock()
}

// RemoveNode 移除节点
func (p *UpstreamPool) RemoveNode(addr string) {
	p.mu.Lock()
	defer p.mu.Unlock()
	for i, n := range p.nodes {
		if n.Addr == addr {
			if n.Client != nil {
				n.Client.Close()
			}
			p.nodes = append(p.nodes[:i], p.nodes[i+1:]...)
			return
		}
	}
}

// Start 启动连接池（尝试初始连接 + 后台重连）
func (p *UpstreamPool) Start() {
	p.mu.Lock()
	for _, n := range p.nodes {
		p.tryConnectLocked(n)
	}
	p.mu.Unlock()

	p.running.Store(true)
	p.wg.Add(1)
	go p.reconnectLoop()
}

// Stop 停止连接池
func (p *UpstreamPool) Stop() {
	if !p.running.CompareAndSwap(true, false) {
		return
	}
	close(p.stopCh)
	p.wg.Wait()

	p.mu.Lock()
	defer p.mu.Unlock()
	for _, n := range p.nodes {
		if n.Client != nil {
			n.Client.Close()
		}
	}
}

// Pick 轮询选择一个健康节点，返回 nil 表示无可用节点
func (p *UpstreamPool) Pick() *Node {
	p.mu.RLock()
	defer p.mu.RUnlock()
	if len(p.nodes) == 0 {
		return nil
	}

	start := int(p.rrIndex.Add(1)) % len(p.nodes)
	for i := 0; i < len(p.nodes); i++ {
		idx := (start + i) % len(p.nodes)
		n := p.nodes[idx]
		if n.Healthy.Load() && n.Client != nil && n.Client.Conn() != nil {
			n.LastActive = time.Now()
			return n
		}
	}
	return nil
}

// SendPacket 发送 Packet 到选中的健康节点
func (p *UpstreamPool) SendPacket(pkt *Packet) bool {
	n := p.Pick()
	if n == nil || n.Client == nil || n.Client.Conn() == nil {
		return false
	}
	return n.Client.Conn().SendPacket(pkt)
}

// HealthyCount 当前健康节点数
func (p *UpstreamPool) HealthyCount() int {
	p.mu.RLock()
	defer p.mu.RUnlock()
	count := 0
	for _, n := range p.nodes {
		if n.Healthy.Load() {
			count++
		}
	}
	return count
}

// TotalCount 总节点数
func (p *UpstreamPool) TotalCount() int {
	p.mu.RLock()
	defer p.mu.RUnlock()
	return len(p.nodes)
}

// AllNodes 返回所有节点地址（用于外部遍历）
func (p *UpstreamPool) AllNodes() []*Node {
	p.mu.RLock()
	defer p.mu.RUnlock()
	out := make([]*Node, len(p.nodes))
	copy(out, p.nodes)
	return out
}

func (p *UpstreamPool) reconnectLoop() {
	defer p.wg.Done()
	interval := time.Second
	maxInterval := 30 * time.Second

	for {
		select {
		case <-p.stopCh:
			return
		case <-time.After(interval):
		}

		anyReconnected := false
		p.mu.Lock()
		for _, n := range p.nodes {
			if !n.Healthy.Load() && !n.Connecting.Load() {
				if p.tryConnectLocked(n) {
					anyReconnected = true
				}
			}
		}
		p.mu.Unlock()

		if anyReconnected {
			interval = time.Second
		} else if p.HealthyCount() == 0 {
			interval = min(interval*2, maxInterval)
		} else {
			interval = time.Second
		}
	}
}

func (p *UpstreamPool) tryConnectLocked(n *Node) bool {
	n.Connecting.Store(true)
	client := NewTCPClient(n.Addr, p.onData, func(_ *TCPConn) {
		n.Healthy.Store(false)
		if p.onNodeEvent != nil {
			p.onNodeEvent(n.Addr, false)
		}
	})
	if err := client.Connect(); err != nil {
		n.Healthy.Store(false)
		n.Connecting.Store(false)
		return false
	}
	n.Client = client
	n.Healthy.Store(true)
	n.LastActive = time.Now()
	n.Connecting.Store(false)
	if p.onNodeEvent != nil {
		p.onNodeEvent(n.Addr, true)
	}
	return true
}

func min(a, b time.Duration) time.Duration {
	if a < b {
		return a
	}
	return b
}
