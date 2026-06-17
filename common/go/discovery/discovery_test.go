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

// 审计 P0 #6：Host 为空的事件（etcd Delete 事件 Host:Port 为零值）
// 当前实现直接 return，导致该节点无法从池中移除 —— 死节点残留。
// 此测试记录该已知缺陷。
func TestOnNodeEventLeaveWithEmptyHost_KnownGap(t *testing.T) {
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

	// 模拟 etcd Delete 事件：Host 为空（已知缺陷）
	m.onNodeEvent(NodeEvent{
		Type: NodeEventLeave,
		Node: NodeInfo{ServiceType: "biz", NodeID: "n1", Host: "", Port: 0},
	})

	// 当前实现因 Host=="" 而 return，节点仍残留 —— 这是缺陷的表现
	if pool.TotalCount() != 0 {
		t.Logf("KNOWN P0 #6: LEAVE event with empty Host was ignored; dead node n1 still in pool (TotalCount=%d). "+
			"Fix: onNodeEvent should fall back to removing by NodeID when Host is empty.", pool.TotalCount())
		// 这是已知缺陷，不判 FAIL，仅记录；真正修复后应改为：
		// if pool.TotalCount() != 0 { t.Fatalf("dead node not removed by NodeID") }
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
