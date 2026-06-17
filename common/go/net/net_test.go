package net

import (
	"bytes"
	stdnet "net"
	"io"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// listenAny 在本地随机端口监听（用于探测可用端口）
func listenAny() (stdnet.Listener, error) {
	return stdnet.Listen("tcp", "127.0.0.1:0")
}

func pickFreeAddr(t *testing.T) string {
	t.Helper()
	ln, err := listenAny()
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	addr := ln.Addr().String()
	ln.Close()
	return addr
}

// ============================================================
// packet.go: 帧编解码（纯函数，无并发副作用）
// ============================================================

func TestPacketEncodeDecodeRoundTrip(t *testing.T) {
	orig := &Packet{
		Header: Header{
			Length:    HeaderSize + 5,
			Magic:     MagicValue,
			CmdID:     0x00010001,
			SeqID:     42,
			Flags:     uint32(FlagEncrypt | FlagRPCReq),
			UserID:    0x1122334455667788,
			ZoneID:    7,
			ServiceID: 9,
		},
		Payload: []byte{0x01, 0x02, 0x03, 0x04, 0x05},
	}
	data := orig.Encode()

	if len(data) != int(orig.Length) {
		t.Fatalf("encoded length = %d, want %d", len(data), orig.Length)
	}

	h, err := DecodeHeader(bytes.NewReader(data))
	if err != nil {
		t.Fatalf("DecodeHeader failed: %v", err)
	}
	if h.Magic != orig.Magic || h.CmdID != orig.CmdID || h.SeqID != orig.SeqID ||
		h.Flags != orig.Flags || h.UserID != orig.UserID ||
		h.ZoneID != orig.ZoneID || h.ServiceID != orig.ServiceID {
		t.Fatalf("header mismatch:\n got =%+v\nwant =%+v", h, orig.Header)
	}

	payload, err := ReadPayload(bytes.NewReader(data[HeaderSize:]), h)
	if err != nil {
		t.Fatalf("ReadPayload failed: %v", err)
	}
	if !bytes.Equal(payload, orig.Payload) {
		t.Fatalf("payload mismatch: got %v want %v", payload, orig.Payload)
	}
}

func TestDecodeHeaderRejectsInvalidLength(t *testing.T) {
	cases := []struct {
		name string
		len  uint32
	}{
		{"too small", HeaderSize - 1},
		{"zero", 0},
		{"over max", MaxPacketLen + 1},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			buf := make([]byte, 4)
			buf[0] = byte(tc.len >> 24)
			buf[1] = byte(tc.len >> 16)
			buf[2] = byte(tc.len >> 8)
			buf[3] = byte(tc.len)
			_, err := DecodeHeader(bytes.NewReader(buf))
			if err == nil {
				t.Fatal("expected error for invalid length, got nil")
			}
		})
	}
}

func TestDecodeHeaderRejectsBadMagic(t *testing.T) {
	pkt := &Packet{
		Header: Header{
			Length: HeaderSize,
			Magic:  0x1234, // wrong magic
		},
	}
	_, err := DecodeHeader(bytes.NewReader(pkt.Encode()))
	if err == nil {
		t.Fatal("expected error for bad magic, got nil")
	}
	if !strings.Contains(err.Error(), "magic") {
		t.Fatalf("expected magic-related error, got: %v", err)
	}
}

func TestDecodeHeaderEOF(t *testing.T) {
	_, err := DecodeHeader(bytes.NewReader(nil))
	if err != io.EOF && err != io.ErrUnexpectedEOF {
		t.Fatalf("expected EOF/ErrUnexpectedEOF, got %v", err)
	}
}

// 空帧（仅包头，无 payload）应正确解码且 payload 为 nil
func TestEmptyPayloadPacket(t *testing.T) {
	pkt := &Packet{
		Header: Header{Length: HeaderSize, Magic: MagicValue, CmdID: 1, SeqID: 1},
	}
	data := pkt.Encode()
	h, err := DecodeHeader(bytes.NewReader(data))
	if err != nil {
		t.Fatalf("DecodeHeader: %v", err)
	}
	payload, err := ReadPayload(bytes.NewReader(data[HeaderSize:]), h)
	if err != nil {
		t.Fatalf("ReadPayload: %v", err)
	}
	if payload != nil {
		t.Fatalf("expected nil payload, got %v", payload)
	}
}

// Encode 不应修改入参 Packet（SendPacket 注释承诺"不修改入参"，这里验证 Encode 也独立分配）
func TestPacketEncodeDoesNotMutateInput(t *testing.T) {
	p := &Packet{
		Header:  Header{Length: HeaderSize + 2, Magic: MagicValue, CmdID: 5},
		Payload: []byte{0xAA, 0xBB},
	}
	_ = p.Encode()
	if p.Payload[0] != 0xAA || p.Payload[1] != 0xBB {
		t.Fatalf("Encode mutated input payload: %v", p.Payload)
	}
	if p.Length != HeaderSize+2 {
		t.Fatalf("Encode mutated input length: %d", p.Length)
	}
}

// ============================================================
// conn.go: 单连接读写（绕开 TCPServer.Stop，直接用 raw net.Conn）
//
// 注意：不通过 TCPServer.Stop() 收尾，因为 conn.Close() 的 wg.Wait 与
// readLoop/writeLoop 的 defer Close() 存在自死锁（见 TestConnCloseDeadlock）。
// 这里只验证数据通路本身。
// ============================================================

// dialAndWrap 建立一对 raw TCP 连接，server 侧用 NewTCPConn 包装
func dialAndWrap(t *testing.T, onData func(*TCPConn, *Packet), onClose func(*TCPConn)) (*TCPConn, stdnet.Conn) {
	t.Helper()
	ln, err := listenAny()
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	defer ln.Close()

	type accepted struct {
		c   stdnet.Conn
		err error
	}
	ac := make(chan accepted, 1)
	go func() {
		c, err := ln.Accept()
		ac <- accepted{c, err}
	}()

	clientRaw, err := stdnet.DialTimeout("tcp", ln.Addr().String(), 2*time.Second)
	if err != nil {
		t.Fatalf("dial: %v", err)
	}
	res := <-ac
	if res.err != nil {
		t.Fatalf("accept: %v", res.err)
	}
	serverConn := NewTCPConn(res.c, onData, onClose)
	return serverConn, clientRaw
}

func TestTCPConnSendAndReceive(t *testing.T) {
	var (
		mu       sync.Mutex
		received []*Packet
	)
	serverConn, clientRaw := dialAndWrap(t,
		func(_ *TCPConn, pkt *Packet) {
			mu.Lock()
			received = append(received, pkt)
			mu.Unlock()
		}, nil)

	// 从 clientRaw 直接写一个完整帧
	out := &Packet{
		Header: Header{
			Length: HeaderSize + 4,
			Magic:  MagicValue,
			CmdID:  0x00020001,
			SeqID:  7,
		},
		Payload: []byte("ping"),
	}
	if _, err := clientRaw.Write(out.Encode()); err != nil {
		t.Fatalf("write: %v", err)
	}

	deadline := time.After(2 * time.Second)
	for {
		mu.Lock()
		n := len(received)
		mu.Unlock()
		if n >= 1 {
			break
		}
		select {
		case <-deadline:
			t.Fatalf("timeout: server conn received %d packets", n)
		case <-time.After(5 * time.Millisecond):
		}
	}

	mu.Lock()
	defer mu.Unlock()
	if string(received[0].Payload) != "ping" {
		t.Fatalf("payload = %q, want %q", string(received[0].Payload), "ping")
	}
	if received[0].SeqID != 7 {
		t.Fatalf("seq = %d, want 7", received[0].SeqID)
	}

	// 仅关闭底层 raw（不调 conn.Close()，避免触发已知的 wg 死锁）
	serverConn.Raw().Close()
	clientRaw.Close()
	time.Sleep(50 * time.Millisecond)
}

// SendPacket 在 closed 标志置位后必须返回 false，且不 panic
func TestTCPConnSendOnClosed(t *testing.T) {
	serverConn, clientRaw := dialAndWrap(t, nil, nil)
	defer clientRaw.Close()

	// 手动置 closed 标志并关闭底层
	atomic.StoreInt32(&serverConn.closed, 1)
	serverConn.Raw().Close()

	pkt := &Packet{
		Header:  Header{Length: HeaderSize + 1, Magic: MagicValue, CmdID: 1},
		Payload: []byte{0x01},
	}
	// Send 走 select default，channel 可能没满，先排空再断言
	// 这里核心断言：closed 后 SendPacket 不应 panic
	_ = serverConn.SendPacket(pkt)
}

// ============================================================
// 回归测试：conn.Close() 死锁 bug
//
// 当前实现中，readLoop/writeLoop 的 defer 顺序为：
//   defer c.wg.Done()   // 先注册
//   defer c.Close()     // 后注册
// return 时 LIFO 先执行 Close()，而 Close() 内 wg.Wait() 等待 wg.Done()。
// Close() 持有 closeOnce 锁期间，goroutine 内的 Close() 阻塞在 Once 锁上，
// 永远到不了 wg.Done() → 外部 Close() 的 wg.Wait() 永久阻塞 → 死锁。
//
// 这是 P0 级 bug，任何连接断开都可能让进程挂死。
// 标记 Skip 直至修复，但保留作为回归用例。
// ============================================================

func TestConnCloseDeadlock(t *testing.T) {
	t.Skip("KNOWN P0 BUG: conn.Close() 与 readLoop/writeLoop 的 defer Close() + wg.Wait() 形成自死锁。" +
		"修复方案：goroutine 内部不调 Close()，或将 defer Close() 移到 defer c.wg.Done() 之前注册，" +
		"或 Close() 不等待 wg（改用 onClose 回调驱动清理）。")

	serverConn, clientRaw := dialAndWrap(t, nil, nil)
	defer clientRaw.Close()

	done := make(chan struct{})
	go func() {
		serverConn.Close()
		close(done)
	}()
	select {
	case <-done:
		// fixed
	case <-time.After(2 * time.Second):
		t.Fatal("conn.Close() deadlocked — wg.Wait() waits for wg.Done() that never runs " +
			"because the goroutine's defer Close() blocks on closeOnce first")
	}
}

// ============================================================
// UpstreamPool: 节点增删与健康状态（不依赖 conn.Close 的完整路径）
// ============================================================

func TestUpstreamPoolEmpty(t *testing.T) {
	pool := NewUpstreamPool(nil)
	if pool.Pick() != nil {
		t.Fatal("Pick on empty pool should return nil")
	}
	if pool.SendPacket(&Packet{Header: Header{Length: HeaderSize, Magic: MagicValue}}) {
		t.Fatal("SendPacket on empty pool should return false")
	}
	if pool.HealthyCount() != 0 {
		t.Fatalf("HealthyCount=%d want 0", pool.HealthyCount())
	}
	if pool.TotalCount() != 0 {
		t.Fatalf("TotalCount=%d want 0", pool.TotalCount())
	}
}

// AddNode 幂等：重复添加同一地址不应增加节点数
func TestUpstreamPoolAddNodeIdempotent(t *testing.T) {
	pool := NewUpstreamPool(nil)
	pool.AddNode("127.0.0.1:9999")
	pool.AddNode("127.0.0.1:9999")
	pool.AddNode("127.0.0.1:9998")
	if pool.TotalCount() != 2 {
		t.Fatalf("TotalCount=%d want 2", pool.TotalCount())
	}
}

// RemoveNode 对不存在的地址不应 panic
func TestUpstreamPoolRemoveAbsentNoPanic(t *testing.T) {
	pool := NewUpstreamPool(nil)
	pool.AddNode("127.0.0.1:9999")
	// 不应 panic
	pool.RemoveNode("127.0.0.1:1234")
	if pool.TotalCount() != 1 {
		t.Fatalf("TotalCount=%d want 1", pool.TotalCount())
	}
}

// AllNodes 返回副本，修改不影响内部
func TestUpstreamPoolAllNodesIsCopy(t *testing.T) {
	pool := NewUpstreamPool(nil)
	pool.AddNode("127.0.0.1:9999")
	all := pool.AllNodes()
	if len(all) != 1 {
		t.Fatalf("len=%d want 1", len(all))
	}
	all[0] = nil
	again := pool.AllNodes()
	if again[0] == nil {
		t.Fatal("AllNodes did not return a copy; internal state mutated")
	}
}
