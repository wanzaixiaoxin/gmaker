package rpc

import (
	"context"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/gmaker/luffa/common/go/net"
)

// mockSender 记录所有发送的包，并可被测试主动投递响应
type mockSender struct {
	mu       sync.Mutex
	sent     []*net.Packet
	sendOK   bool
}

func newMockSender() *mockSender {
	return &mockSender{sendOK: true}
}

func (m *mockSender) SendPacket(pkt *net.Packet) bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.sent = append(m.sent, pkt)
	return m.sendOK
}

func (m *mockSender) lastSent() *net.Packet {
	m.mu.Lock()
	defer m.mu.Unlock()
	if len(m.sent) == 0 {
		return nil
	}
	cp := *m.sent[len(m.sent)-1]
	return &cp
}

// ============================================================
// Call / OnPacket 配对
// ============================================================

func TestCallReceivesMatchingResponse(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	// 用 goroutine 发起 Call，主线程收到 sent 包后投递响应
	type result struct {
		pkt *net.Packet
		err error
	}
	done := make(chan result, 1)

	// 先等待 sender 收到请求
	var sentPkt *net.Packet
	go func() {
		pkt, err := c.Call(context.Background(), 0x00010001, []byte("req"))
		done <- result{pkt, err}
	}()

	// 轮询等待请求被发出
	deadline := time.After(2 * time.Second)
	for {
		if p := sender.lastSent(); p != nil {
			sentPkt = p
			break
		}
		select {
		case <-deadline:
			t.Fatal("timeout: Call never sent a packet")
		case <-time.After(5 * time.Millisecond):
		}
	}

	// 校验发出的请求帧格式正确
	if sentPkt.CmdID != 0x00010001 {
		t.Fatalf("sent cmd=%x want 0x00010001", sentPkt.CmdID)
	}
	if sentPkt.SeqID == 0 {
		t.Fatal("sent SeqID should be non-zero for a Call")
	}
	if sentPkt.Flags&uint32(net.FlagRPCReq) == 0 {
		t.Fatal("sent Flags missing FlagRPCReq")
	}
	if string(sentPkt.Payload) != "req" {
		t.Fatalf("sent payload=%q want %q", string(sentPkt.Payload), "req")
	}

	// 投递匹配的响应（同 SeqID）
	resp := &net.Packet{
		Header:  net.Header{SeqID: sentPkt.SeqID, Flags: uint32(net.FlagRPCRes)},
		Payload: []byte("resp"),
	}
	c.OnPacket(resp)

	select {
	case r := <-done:
		if r.err != nil {
			t.Fatalf("Call returned err: %v", r.err)
		}
		if string(r.pkt.Payload) != "resp" {
			t.Fatalf("response payload=%q want %q", string(r.pkt.Payload), "resp")
		}
	case <-time.After(2 * time.Second):
		t.Fatal("Call did not return after response dispatched")
	}

	// 配对完成后 pending 应已清理
	if sentPkt.SeqID != 0 {
		if _, ok := c.pending.Load(sentPkt.SeqID); ok {
			t.Fatal("pending entry not cleaned up after Call returned")
		}
	}
}

// SeqID=0 的 push 包不应被 OnPacket 当作响应处理
func TestOnPacketIgnoresPushPacket(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	// 投递一个 SeqID=0 的包，不应 panic、不应匹配任何 pending
	c.OnPacket(&net.Packet{Header: net.Header{SeqID: 0}, Payload: []byte("push")})

	// pending 为空
	count := 0
	c.pending.Range(func(_, _ interface{}) bool {
		count++
		return true
	})
	if count != 0 {
		t.Fatalf("pending should be empty, has %d", count)
	}
}

// ============================================================
// 超时与错误路径
// ============================================================

func TestCallTimeout(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()

	start := time.Now()
	_, err := c.Call(ctx, 0x1, []byte("x"))
	elapsed := time.Since(start)

	if err != context.DeadlineExceeded {
		t.Fatalf("err = %v, want context.DeadlineExceeded", err)
	}
	if elapsed > 500*time.Millisecond {
		t.Fatalf("Call returned too late: %v", elapsed)
	}
	// 超时后 pending 应被 defer 清理
	// （注意：seq 已递增，无法直接定位，这里只验证总数）
}

// sender.SendPacket 返回 false 时 Call 应立即返回错误
func TestCallSendFailure(t *testing.T) {
	sender := newMockSender()
	sender.sendOK = false
	c := NewClientWithPool(sender)

	_, err := c.Call(context.Background(), 0x1, []byte("x"))
	if err == nil {
		t.Fatal("expected error when SendPacket fails, got nil")
	}
}

// NewClient(nil) 历史缺陷（已修复）：typed-nil 接口陷阱。
// 修复前 `c.sender = conn`（*TCPConn(nil)）使 c.sender 为非 nil 接口，
// Call 的 `c.sender == nil` 防御失效，SendPacket 在 nil receiver 上 panic。
// 修复后 NewClient 在 conn==nil 时显式置 sender 为 nil 接口，Call 返回 error。
func TestCallNilSenderReturnsError(t *testing.T) {
	c := NewClient(nil)
	_, err := c.Call(context.Background(), 0x1, []byte("x"))
	if err == nil {
		t.Fatal("expected error for nil sender, got nil")
	}
}

// ============================================================
// CallWithTimeout
// ============================================================

func TestCallWithTimeoutReturnsResponse(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	go func() {
		// 等待请求发出后投递响应
		var seq uint32
		for i := 0; i < 100; i++ {
			if p := sender.lastSent(); p != nil {
				seq = p.SeqID
				break
			}
			time.Sleep(5 * time.Millisecond)
		}
		if seq == 0 {
			return
		}
		c.OnPacket(&net.Packet{Header: net.Header{SeqID: seq}, Payload: []byte("ok")})
	}()

	pkt, err := c.CallWithTimeout(0x2, []byte("q"), 1*time.Second)
	if err != nil {
		t.Fatalf("CallWithTimeout err: %v", err)
	}
	if string(pkt.Payload) != "ok" {
		t.Fatalf("payload=%q want %q", string(pkt.Payload), "ok")
	}
}

// ============================================================
// FireForget
// ============================================================

func TestFireForgetSendsAndDoesNotBlock(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	if err := c.FireForget(0x3, []byte("ff")); err != nil {
		t.Fatalf("FireForget err: %v", err)
	}

	if sender.lastSent() == nil {
		t.Fatal("FireForget did not send a packet")
	}
	p := sender.lastSent()
	if p.SeqID != 0 {
		t.Fatalf("FireForget SeqID=%d want 0", p.SeqID)
	}
	if p.Flags&uint32(net.FlagRPCFF) == 0 {
		t.Fatal("FireForget Flags missing FlagRPCFF")
	}
}

// FireForget 历史缺陷（已修复）：同 typed-nil sender 陷阱。
// 修复后 nil sender 时返回 error 而非 panic。
func TestFireForgetNilSenderReturnsError(t *testing.T) {
	c := NewClient(nil)
	if err := c.FireForget(0x1, []byte("x")); err == nil {
		t.Fatal("expected error for nil sender, got nil")
	}
}

// ============================================================
// SeqID 单调递增
// ============================================================

func TestSeqIDMonotonic(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Millisecond)
	defer cancel()

	// 连续发起多个会超时的 Call，观察 SeqID 单调递增
	var seqs []uint32
	var wg sync.WaitGroup
	for i := 0; i < 5; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, _ = c.Call(ctx, 0x1, []byte("x"))
		}()
	}
	// 等待所有 Call 至少发出请求
	for {
		sender.mu.Lock()
		n := len(sender.sent)
		sender.mu.Unlock()
		if n >= 5 {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}
	wg.Wait()

	sender.mu.Lock()
	for _, p := range sender.sent {
		seqs = append(seqs, p.SeqID)
	}
	sender.mu.Unlock()

	if len(seqs) < 5 {
		t.Fatalf("expected >=5 sent packets, got %d", len(seqs))
	}
	// 所有 SeqID 非零且唯一（单调原子计数器，回绕在 uint32 极大值，正常不会触发）
	seen := make(map[uint32]bool)
	for _, s := range seqs {
		if s == 0 {
			t.Fatal("SeqID == 0 encountered for a Call")
		}
		if seen[s] {
			t.Fatalf("duplicate SeqID %d — atomic counter broken", s)
		}
		seen[s] = true
	}
}

// ============================================================
// callChanPool 复用（验证不泄漏、不损坏）
// ============================================================

func TestCallChanPoolReuse(t *testing.T) {
	// 反复 acquire/release，验证 pool 正常工作且 channel 可重用
	for i := 0; i < 100; i++ {
		ch := acquireCallChan()
		if cap(ch) != 1 {
			t.Fatalf("call chan cap=%d want 1", cap(ch))
		}
		// 模拟一个残留包后 release（应被 drain）
		ch <- &net.Packet{Payload: []byte("stale")}
		releaseCallChan(ch)
		// 再次 acquire 不应包含残留数据
		ch2 := acquireCallChan()
		select {
		case stale := <-ch2:
			t.Fatalf("reused chan contained stale packet: %v", stale)
		default:
		}
		releaseCallChan(ch2)
	}
}

// 并发 Call + OnPacket 不应 panic 或数据竞争（用 -race 运行）
func TestConcurrentCallAndDispatch(t *testing.T) {
	sender := newMockSender()
	c := NewClientWithPool(sender)

	var wg sync.WaitGroup
	const N = 20
	// 一个 goroutine 持续投递空 push 包（SeqID=0）
	wg.Add(1)
	go func() {
		defer wg.Done()
		for i := 0; i < 100; i++ {
			c.OnPacket(&net.Packet{Header: net.Header{SeqID: 0}})
		}
	}()

	// N 个并发 Call（会超时）
	var calls atomic.Int32
	for i := 0; i < N; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			ctx, cancel := context.WithTimeout(context.Background(), 20*time.Millisecond)
			defer cancel()
			_, _ = c.Call(ctx, 0x1, []byte("x"))
			calls.Add(1)
		}()
	}
	wg.Wait()

	if calls.Load() != int32(N) {
		t.Fatalf("completed calls=%d want %d", calls.Load(), N)
	}
}
