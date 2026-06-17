package net

import (
	"bufio"
	"log"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/luffa/common/go/crypto"
)

var globalConnID uint64

// TCPConn 封装底层 net.Conn，提供框架统一的读写能力
type TCPConn struct {
	id         uint64
	raw        net.Conn
	reader     *bufio.Reader
	writeCh    chan []byte
	closeCh    chan struct{}
	closed     int32
	onData     func(*TCPConn, *Packet)
	onClose    func(*TCPConn)
	wg         sync.WaitGroup
	closeOnce  sync.Once
	sessionKey []byte // 若非空则启用 AES-GCM 加密

	lastActive       time.Time
	heartbeatTimeout time.Duration
	hbMu             sync.RWMutex
}

// NewTCPConn 创建封装连接
func NewTCPConn(c net.Conn, onData func(*TCPConn, *Packet), onClose func(*TCPConn)) *TCPConn {
	conn := &TCPConn{
		id:         atomic.AddUint64(&globalConnID, 1),
		raw:        c,
		reader:     bufio.NewReader(c),
		writeCh:    make(chan []byte, 256),
		closeCh:    make(chan struct{}),
		onData:     onData,
		onClose:    onClose,
		lastActive: time.Now(),
	}
	conn.wg.Add(2)
	go conn.readLoop()
	go conn.writeLoop()
	return conn
}

// ID 返回连接唯一标识
func (c *TCPConn) ID() uint64 {
	return c.id
}

// Raw 返回底层 net.Conn
func (c *TCPConn) Raw() net.Conn {
	return c.raw
}

// SetHeartbeatTimeout 设置心跳超时时间（0 表示不检测）
func (c *TCPConn) SetHeartbeatTimeout(d time.Duration) {
	c.hbMu.Lock()
	c.heartbeatTimeout = d
	c.hbMu.Unlock()
}

func (c *TCPConn) isHeartbeatExpired() bool {
	c.hbMu.RLock()
	timeout := c.heartbeatTimeout
	last := c.lastActive
	c.hbMu.RUnlock()
	if timeout <= 0 {
		return false
	}
	return time.Since(last) > timeout
}

func (c *TCPConn) updateLastActive() {
	c.hbMu.Lock()
	c.lastActive = time.Now()
	c.hbMu.Unlock()
}

// Send 异步发送字节流（要求外部已编码）
func (c *TCPConn) Send(data []byte) bool {
	if atomic.LoadInt32(&c.closed) == 1 {
		return false
	}
	select {
	case c.writeCh <- data:
		return true
	default:
		return false
	}
}

// SendPacket 发送一个 Packet（若有 sessionKey 则自动加密）
// 注意：本方法不修改入参 p，调用方可安全复用 Packet 对象
func (c *TCPConn) SendPacket(p *Packet) bool {
	encPkt := *p // copy header to avoid mutating caller's packet
	if len(c.sessionKey) > 0 && len(p.Payload) > 0 {
		enc, err := crypto.EncryptPacketPayload(c.sessionKey, p.Payload)
		if err != nil {
			return false
		}
		encPkt.Payload = enc
		encPkt.Flags |= uint32(FlagEncrypt)
	}
	encPkt.Length = HeaderSize + uint32(len(encPkt.Payload))
	return c.Send(encPkt.Encode())
}

// shutdown 关闭底层连接、关闭 closeCh 并触发 onClose 回调（仅一次），
// 但不等待 goroutine 退出。readLoop/writeLoop 的 defer 安全调用它：
// 它们需要触发关闭与清理回调，但不能等待 wg.Done()（否则与
// 外部 Close 的 wg.Wait() 自死锁）。回调在 closeOnce 内执行，
// 因此无论由谁触发关闭，onClose 都恰好执行一次。
func (c *TCPConn) shutdown() {
	c.closeOnce.Do(func() {
		atomic.StoreInt32(&c.closed, 1)
		close(c.closeCh)
		if c.raw != nil {
			c.raw.Close()
		}
		if c.onClose != nil {
			c.onClose(c)
		}
	})
}

// Close 优雅关闭连接：触发关闭（含 onClose 回调），并等待所有
// goroutine 退出。可被多次调用。外部调用者等待 readLoop/writeLoop 退出。
func (c *TCPConn) Close() {
	c.shutdown()
	c.wg.Wait()
}

func (c *TCPConn) readLoop() {
	defer c.wg.Done()
	defer c.shutdown()
	defer func() {
		if r := recover(); r != nil {
			log.Printf("readLoop panic recovered on conn %d: %v", c.id, r)
		}
	}()
	for {
		select {
		case <-c.closeCh:
			return
		default:
		}

		if c.isHeartbeatExpired() {
			return
		}

		c.raw.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
		h, err := DecodeHeader(c.reader)
		if err != nil {
			c.raw.SetReadDeadline(time.Time{})
			if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
				continue
			}
			return
		}
		c.raw.SetReadDeadline(time.Time{})
		payload, err := ReadPayload(c.reader, h)
		if err != nil {
			return
		}

		c.updateLastActive()

		pkt := &Packet{
			Header:  *h,
			Payload: payload,
		}
		if len(c.sessionKey) > 0 && (pkt.Flags&uint32(FlagEncrypt)) != 0 {
			dec, err := crypto.DecryptPacketPayload(c.sessionKey, pkt.Payload)
			if err != nil {
				return
			}
			pkt.Payload = dec
			pkt.Flags &= ^uint32(FlagEncrypt)
		}
		if c.onData != nil {
			c.onData(c, pkt)
		}
	}
}

func (c *TCPConn) writeLoop() {
	defer c.wg.Done()
	defer c.shutdown()
	defer func() {
		if r := recover(); r != nil {
			log.Printf("writeLoop panic recovered on conn %d: %v", c.id, r)
		}
	}()
	for {
		select {
		case data := <-c.writeCh:
			c.raw.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if _, err := c.raw.Write(data); err != nil {
				return
			}
		case <-c.closeCh:
			// drain remaining
			for {
				select {
				case data := <-c.writeCh:
					c.raw.SetWriteDeadline(time.Now().Add(5 * time.Second))
					c.raw.Write(data)
				default:
					return
				}
			}
		}
	}
}
