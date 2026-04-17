package net

import (
	"bufio"
	"net"
	"sync"
	"sync/atomic"

	"github.com/gmaker/game-server/common/go/crypto"
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
	sessionKey []byte // 若非空则启用 AES-GCM 加密
}

// NewTCPConn 创建封装连接
func NewTCPConn(c net.Conn, onData func(*TCPConn, *Packet), onClose func(*TCPConn)) *TCPConn {
	conn := &TCPConn{
		id:      atomic.AddUint64(&globalConnID, 1),
		raw:     c,
		reader:  bufio.NewReader(c),
		writeCh: make(chan []byte, 256),
		closeCh: make(chan struct{}),
		onData:  onData,
		onClose: onClose,
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
func (c *TCPConn) SendPacket(p *Packet) bool {
	if len(c.sessionKey) > 0 && len(p.Payload) > 0 {
		enc, err := crypto.EncryptPacketPayload(c.sessionKey, p.Payload)
		if err != nil {
			return false
		}
		p.Payload = enc
		p.Flags |= uint32(FlagEncrypt)
	}
	p.Length = HeaderSize + uint32(len(p.Payload))
	return c.Send(p.Encode())
}

// Close 优雅关闭连接
func (c *TCPConn) Close() {
	if atomic.CompareAndSwapInt32(&c.closed, 0, 1) {
		close(c.closeCh)
		c.raw.Close()
		c.wg.Wait()
		if c.onClose != nil {
			c.onClose(c)
		}
	}
}

func (c *TCPConn) readLoop() {
	defer c.wg.Done()
	for {
		select {
		case <-c.closeCh:
			return
		default:
		}

		h, err := DecodeHeader(c.reader)
		if err != nil {
			c.Close()
			return
		}
		payload, err := ReadPayload(c.reader, h)
		if err != nil {
			c.Close()
			return
		}

		pkt := &Packet{
			Header:  *h,
			Payload: payload,
		}
		if len(c.sessionKey) > 0 && (pkt.Flags&uint32(FlagEncrypt)) != 0 {
			dec, err := crypto.DecryptPacketPayload(c.sessionKey, pkt.Payload)
			if err != nil {
				c.Close()
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
	for {
		select {
		case data := <-c.writeCh:
			if _, err := c.raw.Write(data); err != nil {
				c.Close()
				return
			}
		case <-c.closeCh:
			// drain remaining
			for {
				select {
				case data := <-c.writeCh:
					c.raw.Write(data)
				default:
					return
				}
			}
		}
	}
}
