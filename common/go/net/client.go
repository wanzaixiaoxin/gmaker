package net

import (
	"encoding/binary"
	"fmt"
	"net"
	"time"

	"github.com/gmaker/luffa/common/go/crypto"
)

// TCPClient TCP 客户端
type TCPClient struct {
	addr      string
	conn      *TCPConn
	onData    func(*TCPConn, *Packet)
	onClose   func(*TCPConn)
	masterKey []byte
}

// NewTCPClient 创建客户端
func NewTCPClient(addr string, onData func(*TCPConn, *Packet), onClose func(*TCPConn)) *TCPClient {
	return &TCPClient{
		addr:    addr,
		onData:  onData,
		onClose: onClose,
	}
}

// SetMasterKey 设置加密主密钥（32 字节），设置后连接会自动进行握手加密
func (c *TCPClient) SetMasterKey(key []byte) {
	c.masterKey = key
}

// Connect 建立连接
func (c *TCPClient) Connect() error {
	raw, err := net.DialTimeout("tcp", c.addr, 5*time.Second)
	if err != nil {
		return err
	}
	onClose := func(conn *TCPConn) {
		c.conn = nil
		if c.onClose != nil {
			c.onClose(conn)
		}
	}
	c.conn = NewTCPConn(raw, c.onData, onClose)

	if len(c.masterKey) > 0 {
		if err := c.doHandshake(); err != nil {
			c.conn.Close()
			c.conn = nil
			return fmt.Errorf("handshake failed: %w", err)
		}
	}
	return nil
}

func (c *TCPClient) doHandshake() error {
	// HandshakeReq v1: [version: 1][timestamp: 8 BE][nonce: 8][client_random: 16] = 33 bytes
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

	req := &Packet{
		Header: Header{
			Length: HeaderSize + uint32(len(payload)),
			Magic:  MagicValue,
			CmdID:  0x00000002, // HANDSHAKE
			SeqID:  1,
			Flags:  0,
		},
		Payload: payload,
	}
	if !c.conn.SendPacket(req) {
		return fmt.Errorf("send handshake req failed")
	}

	// 同步等待握手响应（最多 5 秒）
	type result struct {
		pkt *Packet
		err error
	}
	done := make(chan result, 1)
	origOnData := c.onData
	c.conn.onData = func(conn *TCPConn, pkt *Packet) {
		if pkt.CmdID == 0x00000002 {
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
func (c *TCPClient) Conn() *TCPConn {
	return c.conn
}

// Close 关闭连接
func (c *TCPClient) Close() {
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
}
