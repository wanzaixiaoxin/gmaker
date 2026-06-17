package discovery

import (
	"context"
	"sync"
	"testing"

	"github.com/gmaker/luffa/common/go/net"
)

// ============================================================
// UpstreamManager: AddInterest / GetPool / Start 空订阅
// ============================================================

// 使用 nil ServiceDiscovery 的桩：我们只测 UpstreamManager 自身的簿记逻辑，
// 不触发真正的 Discover/Watch（那需要真实 Registry）。
type noopSD struct{}

func (noopSD) Register(context.Context, NodeInfo) error                 { return nil }
func (noopSD) Deregister(context.Context, string) error                 { return nil }
func (noopSD) Discover(context.Context, string) ([]NodeInfo, error)     { return nil, nil }
func (noopSD) Watch(context.Context, []string, func(NodeEvent)) error   { return nil }
func (noopSD) Close() error                                             { return nil }

// AddInterest 应为每个 service_type 创建独立连接池
func TestUpstreamManagerAddInterest(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	p1 := m.AddInterest("biz", func(*net.TCPConn, *net.Packet) {})
	p2 := m.AddInterest("realtime", nil)

	if p1 == nil || p2 == nil {
		t.Fatal("AddInterest returned nil pool")
	}
	if m.GetPool("biz") != p1 {
		t.Fatal("GetPool(biz) returned different pool than AddInterest")
	}
	if m.GetPool("realtime") != p2 {
		t.Fatal("GetPool(realtime) returned different pool")
	}
	if m.GetPool("unknown") != nil {
		t.Fatal("GetPool for unregistered service should return nil")
	}
}

// ============================================================
// onNodeEvent: 节点增删路由
// 这是审计 P0 #6 的核心逻辑：Host 为空的事件如何处理
// ============================================================

// 测试 JOIN/LEAVE 事件正常增删节点
func TestOnNodeEventJoinAndLeave(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	pool := m.AddInterest("biz", nil)

	// JOIN 一个节点
	m.onNodeEvent(NodeEvent{
		Type: NodeEventJoin,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "127.0.0.1", Port: 8082},
	})
	if pool.TotalCount() != 1 {
		t.Fatalf("after JOIN, TotalCount=%d want 1", pool.TotalCount())
	}

	// LEAVE 同一节点
	m.onNodeEvent(NodeEvent{
		Type: NodeEventLeave,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "127.0.0.1", Port: 8082},
	})
	if pool.TotalCount() != 0 {
		t.Fatalf("after LEAVE, TotalCount=%d want 0", pool.TotalCount())
	}
}

// 审计 P0 #6（已修复）：Host 为空的 LEAVE 事件（etcd Delete 事件 Host:Port 为零值）。
// 修复前：Host=="" 时直接 return，死节点残留。
// 修复后：JOIN/UPDATE 时记录 nodeID->addr 映射，LEAVE 时按 NodeID 回退定位并移除。
func TestOnNodeEventLeaveWithEmptyHost(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	pool := m.AddInterest("biz", nil)

	// 先正常 JOIN 一个节点（Host 非空）
	m.onNodeEvent(NodeEvent{
		Type: NodeEventJoin,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "127.0.0.1", Port: 8082},
	})
	if pool.TotalCount() != 1 {
		t.Fatalf("after JOIN, TotalCount=%d want 1", pool.TotalCount())
	}

	// 模拟 etcd Delete 事件：Host 为空，但 NodeID 可用
	m.onNodeEvent(NodeEvent{
		Type: NodeEventLeave,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "", Port: 0},
	})

	// 修复后死节点应被按 NodeID 移除
	if pool.TotalCount() != 0 {
		t.Fatalf("after LEAVE with empty Host, dead node n1 still in pool (TotalCount=%d), "+
			"expected removed by NodeID fallback", pool.TotalCount())
	}

	// nodeID->addr 映射应已清理
	m.mu.RLock()
	_, stillMapped := m.nodeAddrs["biz/n1"]
	m.mu.RUnlock()
	if stillMapped {
		t.Fatal("nodeAddrs mapping not cleaned after LEAVE")
	}
}

// Host 为空的 JOIN/UPDATE 事件（无地址）应被忽略，不 panic
func TestOnNodeEventJoinWithEmptyHostIgnored(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	pool := m.AddInterest("biz", nil)

	m.onNodeEvent(NodeEvent{
		Type: NodeEventJoin,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "", Port: 0},
	})
	if pool.TotalCount() != 0 {
		t.Fatalf("JOIN with empty Host should be ignored, TotalCount=%d", pool.TotalCount())
	}
}

// 未记录过的节点收到 Host 空 LEAVE 事件，应安全忽略（不 panic）
func TestOnNodeEventLeaveEmptyHostUnknownNode(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	pool := m.AddInterest("biz", nil)

	// 从未 JOIN 过 n2，却收到它的 Host 空 LEAVE —— 应安全无操作
	m.onNodeEvent(NodeEvent{
		Type: NodeEventLeave,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n2", Host: "", Port: 0},
	})
	if pool.TotalCount() != 0 {
		t.Fatalf("unexpected node in pool: TotalCount=%d", pool.TotalCount())
	}
}

// 未知服务类型的事件应被静默忽略
func TestOnNodeEventUnknownServiceIgnored(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	_ = m.AddInterest("biz", nil)

	// 对未 AddInterest 的服务类型发 JOIN，不应 panic
	m.onNodeEvent(NodeEvent{
		Type: NodeEventJoin,
		Node: NodeInfo{ServiceType: "chat", NodeID: "n1", Host: "127.0.0.1", Port: 8083},
	})
	if m.GetPool("chat") != nil {
		t.Fatal("onNodeEvent created a pool for unregistered service type")
	}
}

// UPDATE 事件应等价于 JOIN（幂等添加）
func TestOnNodeEventUpdateIdempotent(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	pool := m.AddInterest("biz", nil)

	addr := NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "127.0.0.1", Port: 8082}
	m.onNodeEvent(NodeEvent{Type: NodeEventJoin, Node: addr})
	m.onNodeEvent(NodeEvent{Type: NodeEventUpdate, Node: addr})

	// AddNode 幂等，UPDATE 同地址不应翻倍
	if pool.TotalCount() != 1 {
		t.Fatalf("after JOIN+UPDATE same addr, TotalCount=%d want 1", pool.TotalCount())
	}
}

// ============================================================
// 并发：多 service_type 同时 AddInterest/GetPool（用 -race）
// ============================================================

func TestUpstreamManagerConcurrentAccess(t *testing.T) {
	m := NewUpstreamManager(noopSD{})
	var wg sync.WaitGroup
	for i := 0; i < 10; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			svc := "svc-" + string(rune('A'+i))
			m.AddInterest(svc, nil)
			_ = m.GetPool(svc)
		}(i)
	}
	wg.Wait()

	// 不应 panic，且至少注册了若干池
	count := 0
	for i := 0; i < 10; i++ {
		if p := m.GetPool("svc-" + string(rune('A'+i))); p != nil {
			count++
		}
	}
	if count == 0 {
		t.Fatal("no pools registered after concurrent AddInterest")
	}
}
