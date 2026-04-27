package main

import (
	"encoding/binary"
	"fmt"
	"net/url"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
	"github.com/gmaker/luffa/common/go/crypto"
	gsnet "github.com/gmaker/luffa/common/go/net"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
)

var wsConnIDCounter uint64

// WSClient WebSocket 客户端，API 与 TCPClient 对齐
type WSClient struct {
	addr      string
	conn      *WSConn
	onData    func(*WSConn, *gsnet.Packet)
	onClose   func(*WSConn)
	masterKey []byte
}

// NewWSClient 创建 WebSocket 客户端
func NewWSClient(addr string, onData func(*WSConn, *gsnet.Packet), onClose func(*WSConn)) *WSClient {
	return &WSClient{
		addr:    addr,
		onData:  onData,
		onClose: onClose,
	}
}

// SetMasterKey 设置加密主密钥
func (c *WSClient) SetMasterKey(key []byte) { c.masterKey = key }

// Connect 建立 WebSocket 连接并完成 gmaker 握手
func (c *WSClient) Connect() error {
	u := url.URL{Scheme: "ws", Host: c.addr, Path: "/"}
	dialer := websocket.Dialer{HandshakeTimeout: 5 * time.Second}
	raw, _, err := dialer.Dial(u.String(), nil)
	if err != nil {
		return err
	}
	onClose := func(conn *WSConn) {
		c.conn = nil
		if c.onClose != nil {
			c.onClose(conn)
		}
	}
	c.conn = NewWSConn(raw, c.onData, onClose)

	if len(c.masterKey) > 0 {
		if err := c.doHandshake(); err != nil {
			c.conn.Close()
			c.conn = nil
			return fmt.Errorf("handshake failed: %w", err)
		}
	}
	return nil
}

func (c *WSClient) doHandshake() error {
	clientRandom, err := crypto.RandomBytes(16)
	if err != nil {
		return err
	}
	nonce, err := crypto.RandomBytes(8)
	if err != nil {
		return err
	}

	payload := make([]byte, 1+8+8+16)
	payload[0] = 1 // version
	binary.BigEndian.PutUint64(payload[1:9], uint64(time.Now().Unix()))
	copy(payload[9:17], nonce)
	copy(payload[17:], clientRandom)

	req := &gsnet.Packet{
		Header: gsnet.Header{
			Length: gsnet.HeaderSize + uint32(len(payload)),
			Magic:  gsnet.MagicValue,
			CmdID:  uint32(protocol.CmdSystem_CMD_SYS_HANDSHAKE),
			SeqID:  1,
			Flags:  0,
		},
		Payload: payload,
	}
	if !c.conn.SendPacket(req) {
		return fmt.Errorf("send handshake req failed")
	}

	type result struct {
		pkt *gsnet.Packet
		err error
	}
	done := make(chan result, 1)
	origOnData := c.onData
	c.conn.onData = func(conn *WSConn, pkt *gsnet.Packet) {
		if pkt.CmdID == uint32(protocol.CmdSystem_CMD_SYS_HANDSHAKE) {
			done <- result{pkt: pkt}
		} else if origOnData != nil {
			origOnData(conn, pkt)
		}
	}
	defer func() { c.conn.onData = origOnData }()

	select {
	case res := <-done:
		if res.err != nil {
			return res.err
		}
		if len(res.pkt.Payload) < 1+16 {
			return fmt.Errorf("handshake response too short")
		}
		version := res.pkt.Payload[0]
		if version != 1 {
			return fmt.Errorf("unsupported handshake version: %d", version)
		}
		serverRandom := res.pkt.Payload[1:17]
		encryptedChallenge := res.pkt.Payload[17:]

		sessionKey, err := crypto.DeriveSessionKey(c.masterKey, clientRandom, serverRandom)
		if err != nil {
			return err
		}

		challenge, err := crypto.DecryptPacketPayload(sessionKey, encryptedChallenge)
		if err != nil {
			return fmt.Errorf("decrypt challenge failed: %w", err)
		}
		if string(challenge) != string(clientRandom) {
			return fmt.Errorf("handshake challenge mismatch")
		}
		c.conn.sessionKey = sessionKey
		return nil
	case <-time.After(5 * time.Second):
		return fmt.Errorf("handshake timeout")
	}
}

// Conn 返回当前连接
func (c *WSClient) Conn() *WSConn { return c.conn }

// Close 关闭连接
func (c *WSClient) Close() {
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
}

// WSConn WebSocket 连接封装，提供与 TCPConn 对齐的 Send/SendPacket/Close 接口
type WSConn struct {
	id         uint64
	ws         *websocket.Conn
	writeCh    chan []byte
	closeCh    chan struct{}
	closed     int32
	onData     func(*WSConn, *gsnet.Packet)
	onClose    func(*WSConn)
	wg         sync.WaitGroup
	closeOnce  sync.Once
	sessionKey []byte // 若非空则启用 AES-GCM 加密
}

// NewWSConn 创建 WSConn
func NewWSConn(ws *websocket.Conn, onData func(*WSConn, *gsnet.Packet), onClose func(*WSConn)) *WSConn {
	conn := &WSConn{
		id:      atomic.AddUint64(&wsConnIDCounter, 1),
		ws:      ws,
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
func (c *WSConn) ID() uint64 { return c.id }

// Send 异步发送字节流（要求外部已编码为完整帧 payload）
func (c *WSConn) Send(data []byte) bool {
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
func (c *WSConn) SendPacket(p *gsnet.Packet) bool {
	encPkt := *p // copy header
	if len(c.sessionKey) > 0 && len(p.Payload) > 0 {
		enc, err := crypto.EncryptPacketPayload(c.sessionKey, p.Payload)
		if err != nil {
			return false
		}
		encPkt.Payload = enc
		encPkt.Flags |= uint32(gsnet.FlagEncrypt)
	}
	encPkt.Length = gsnet.HeaderSize + uint32(len(encPkt.Payload))
	return c.Send(encPkt.Encode())
}

// doClose 内部关闭（可由 readLoop/writeLoop 或外部调用，仅执行一次）
func (c *WSConn) doClose() {
	c.closeOnce.Do(func() {
		atomic.StoreInt32(&c.closed, 1)
		close(c.closeCh)
		c.ws.Close()
	})
}

// Close 优雅关闭连接
func (c *WSConn) Close() {
	c.doClose()
	c.wg.Wait()
	if c.onClose != nil {
		c.onClose(c)
	}
}

func (c *WSConn) readLoop() {
	defer c.wg.Done()

	// 当 closeCh 关闭时，强制中断 ReadMessage，避免 readLoop 长期阻塞
	interrupt := make(chan struct{})
	go func() {
		select {
		case <-c.closeCh:
			c.ws.Close()
		case <-interrupt:
		}
	}()
	defer close(interrupt)

	for {
		_, data, err := c.ws.ReadMessage()
		if err != nil {
			if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
				return
			}
			// 其他错误（包括 doClose 触发的连接关闭）直接退出
			return
		}

		if len(data) < gsnet.HeaderSize {
			c.doClose()
			return
		}
		length := binary.BigEndian.Uint32(data[0:4])
		if length < gsnet.HeaderSize || length > gsnet.MaxPacketLen || int(length) > len(data) {
			c.doClose()
			return
		}
		magic := binary.BigEndian.Uint16(data[4:6])
		if magic != gsnet.MagicValue {
			c.doClose()
			return
		}
		h := gsnet.Header{
			Length: length,
			Magic:  magic,
			CmdID:  binary.BigEndian.Uint32(data[6:10]),
			SeqID:  binary.BigEndian.Uint32(data[10:14]),
			Flags:  binary.BigEndian.Uint32(data[14:18]),
		}
		var payload []byte
		payloadLen := int(length) - gsnet.HeaderSize
		if payloadLen > 0 {
			payload = make([]byte, payloadLen)
			copy(payload, data[gsnet.HeaderSize:length])
		}

		pkt := &gsnet.Packet{Header: h, Payload: payload}
		if len(c.sessionKey) > 0 && (pkt.Flags&uint32(gsnet.FlagEncrypt)) != 0 {
			dec, err := crypto.DecryptPacketPayload(c.sessionKey, pkt.Payload)
			if err != nil {
				c.doClose()
				return
			}
			pkt.Payload = dec
			pkt.Flags &^= uint32(gsnet.FlagEncrypt)
		}
		if c.onData != nil {
			c.onData(c, pkt)
		}
	}
}

func (c *WSConn) writeLoop() {
	defer c.wg.Done()
	for {
		select {
		case data := <-c.writeCh:
			if err := c.ws.WriteMessage(websocket.BinaryMessage, data); err != nil {
				c.doClose()
				return
			}
		case <-c.closeCh:
			// drain remaining
			for {
				select {
				case data := <-c.writeCh:
					c.ws.WriteMessage(websocket.BinaryMessage, data)
				default:
					return
				}
			}
		}
	}
}
