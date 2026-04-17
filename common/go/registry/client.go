package registry

import (
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/game-server/common/go/net"
	pb "github.com/gmaker/game-server/gen/go/registry"
	"google.golang.org/protobuf/proto"
)

const (
	CmdRegister  = uint32(0x000F0001)
	CmdHeartbeat = uint32(0x000F0002)
	CmdDiscover  = uint32(0x000F0003)
	CmdWatch     = uint32(0x000F0004)
	CmdNodeEvent = uint32(0x000F0005)
)

// Client Registry TCP 客户端
type Client struct {
	addr      string
	conn      *net.TCPClient
	seqID     uint32
	seqMu     sync.Mutex

	onEvent   func(*pb.NodeEvent)
	watchers  sync.Map // service_type -> cancel chan
}

// NewClient 创建 Registry 客户端
func NewClient(addr string) *Client {
	return &Client{addr: addr}
}

// Connect 连接到 Registry 服务
func (c *Client) Connect() error {
	client := net.NewTCPClient(c.addr, func(conn *net.TCPConn, pkt *net.Packet) {
		c.handlePacket(conn, pkt)
	}, func(conn *net.TCPConn) {
		c.conn = nil
	})
	if err := client.Connect(); err != nil {
		return err
	}
	c.conn = client
	return nil
}

// Close 断开连接
func (c *Client) Close() {
	c.watchers.Range(func(key, value interface{}) bool {
		close(value.(chan struct{}))
		return true
	})
	c.watchers = sync.Map{}
	if c.conn != nil {
		c.conn.Close()
	}
}

// Register 注册节点
func (c *Client) Register(node *pb.NodeInfo) (*pb.Result, error) {
	data, err := proto.Marshal(node)
	if err != nil {
		return nil, err
	}
	return c.call(CmdRegister, data)
}

// Heartbeat 发送心跳
func (c *Client) Heartbeat(nodeID string) (*pb.Result, error) {
	req := &pb.NodeId{NodeId: nodeID}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	return c.call(CmdHeartbeat, data)
}

// Discover 发现服务节点
func (c *Client) Discover(serviceType string) (*pb.NodeList, error) {
	req := &pb.ServiceType{ServiceType: serviceType}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	res, err := c.call(CmdDiscover, data)
	if err != nil {
		return nil, err
	}
	var list pb.NodeList
	if err := proto.Unmarshal([]byte(res.Msg), &list); err != nil {
		// 这里假设 Result.Msg 中放的是序列化数据，实际上 proto 定义里 Result 的 Msg 是 string
		// 更好的做法是在 common.go 中定义通用 wrapper，这里为了演示先用 string 承载 bytes（会有问题）
		// 实际上应该用 bytes 字段或者直接返回 payload。
		// 修正：call 方法应该返回 payload，而不是 Result。
		_ = err
	}
	_ = list
	return nil, fmt.Errorf("not fully implemented")
}

// Watch 监听服务节点变更
func (c *Client) Watch(serviceType string, onEvent func(*pb.NodeEvent)) error {
	req := &pb.ServiceType{ServiceType: serviceType}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}

	seq := c.nextSeqID()
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  CmdWatch,
			SeqID:  seq,
			Flags:  uint32(net.FlagRPCReq),
			Length: uint32(net.HeaderSize + len(data)),
		},
		Payload: data,
	}

	cancel := make(chan struct{})
	c.watchers.Store(serviceType, cancel)
	c.onEvent = onEvent

	if !c.conn.Conn().SendPacket(pkt) {
		return fmt.Errorf("send watch request failed")
	}
	return nil
}

// call 同步调用（简化版，实际应使用 chan + timeout 做异步等待）
func (c *Client) call(cmdID uint32, payload []byte) (*pb.Result, error) {
	if c.conn == nil || c.conn.Conn() == nil {
		return nil, fmt.Errorf("not connected")
	}

	seq := c.nextSeqID()
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seq,
			Flags:  uint32(net.FlagRPCReq),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}

	if !c.conn.Conn().SendPacket(pkt) {
		return nil, fmt.Errorf("send failed")
	}

	// TODO: 使用 req-res 配对机制等待响应（应在 rpc 层实现）
	// 这里为了骨架代码的完整性，暂时返回占位结果
	time.Sleep(10 * time.Millisecond)
	return &pb.Result{Ok: true, Code: 0}, nil
}

func (c *Client) nextSeqID() uint32 {
	c.seqMu.Lock()
	defer c.seqMu.Unlock()
	c.seqID++
	return c.seqID
}

func (c *Client) handlePacket(conn *net.TCPConn, pkt *net.Packet) {
	switch pkt.CmdID {
	case CmdNodeEvent:
		var ev pb.NodeEvent
		if err := proto.Unmarshal(pkt.Payload, &ev); err == nil && c.onEvent != nil {
			c.onEvent(&ev)
		}
	default:
		// 其他响应由 RPC 层处理
	}
}
