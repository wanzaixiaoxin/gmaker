package registry

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/registry"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	CmdRegister  = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_REGISTER)
	CmdHeartbeat = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_HEARTBEAT)
	CmdDiscover  = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_DISCOVER)
	CmdWatch     = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_WATCH)
	CmdNodeEvent = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_NODE_EVENT)
	CmdSubscribe = uint32(protocol.CmdRegistryInternal_CMD_REG_INT_SUBSCRIBE)
)

// Client Registry TCP 客户端，支持多节点连接池
type Client struct {
	pool    *net.UpstreamPool
	seqID   uint32
	seqMu   sync.Mutex
	pending sync.Map // seq_id -> chan *net.Packet

	onEvent    func(*pb.NodeEvent)
	watchTypes sync.Map // service_type -> struct{}{}
}

// NewClient 创建 Registry 客户端
// addrs: Registry 节点地址列表，如 ["127.0.0.1:2379", "127.0.0.1:2380"]
func NewClient(addrs []string) *Client {
	c := &Client{}
	c.pool = net.NewUpstreamPool(c.handlePacket)
	c.pool.SetOnNodeEvent(func(addr string, healthy bool) {
		if healthy {
			// 节点恢复后，重新发送所有 Watch 请求
			c.watchTypes.Range(func(key, _ interface{}) bool {
				c.sendWatch(key.(string))
				return true
			})
		}
	})
	for _, addr := range addrs {
		c.pool.AddNode(addr)
	}
	return c
}

// Connect 启动连接池
func (c *Client) Connect() error {
	c.pool.Start()
	return nil
}

// Close 断开连接
func (c *Client) Close() {
	c.pool.Stop()
	c.pending.Range(func(key, value interface{}) bool {
		close(value.(chan *net.Packet))
		return true
	})
}

// Register 注册节点
func (c *Client) Register(node *pb.NodeInfo) (*pb.Result, error) {
	data, err := proto.Marshal(node)
	if err != nil {
		return nil, err
	}
	pkt, err := c.call(context.Background(), CmdRegister, data)
	if err != nil {
		return nil, err
	}
	var res pb.Result
	if err := proto.Unmarshal(pkt.Payload, &res); err != nil {
		return nil, err
	}
	return &res, nil
}

// RegisterWithRetry 带退避重试的注册，适用于服务启动时 Registry 尚未就绪的场景
func (c *Client) RegisterWithRetry(node *pb.NodeInfo, maxRetries int) (*pb.Result, error) {
	var lastErr error
	for i := 0; i < maxRetries; i++ {
		res, err := c.Register(node)
		if err == nil {
			return res, nil
		}
		lastErr = err
		if i < maxRetries-1 {
			time.Sleep(time.Duration(i+1) * time.Second)
		}
	}
	return nil, lastErr
}

// Heartbeat 发送心跳
func (c *Client) Heartbeat(nodeID string) (*pb.Result, error) {
	req := &pb.NodeId{NodeId: nodeID}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	pkt, err := c.call(context.Background(), CmdHeartbeat, data)
	if err != nil {
		return nil, err
	}
	var res pb.Result
	if err := proto.Unmarshal(pkt.Payload, &res); err != nil {
		return nil, err
	}
	return &res, nil
}

// Discover 发现服务节点
func (c *Client) Discover(serviceType string) (*pb.NodeList, error) {
	req := &pb.ServiceType{ServiceType: serviceType}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	pkt, err := c.call(context.Background(), CmdDiscover, data)
	if err != nil {
		return nil, err
	}
	var list pb.NodeList
	if err := proto.Unmarshal(pkt.Payload, &list); err != nil {
		return nil, err
	}
	return &list, nil
}

// Watch 监听单个服务类型节点变更（兼容旧接口）
func (c *Client) Watch(serviceType string, onEvent func(*pb.NodeEvent)) error {
	c.onEvent = onEvent
	c.watchTypes.Store(serviceType, struct{}{})
	return c.sendWatch(serviceType)
}

// Subscribe 批量订阅多个服务类型，返回当前全量快照，后续增量通过 onEvent 推送
func (c *Client) Subscribe(serviceTypes []string, onEvent func(*pb.NodeEvent)) (*pb.SubscribeRes, error) {
	c.onEvent = onEvent
	for _, svc := range serviceTypes {
		c.watchTypes.Store(svc, struct{}{})
	}

	req := &pb.SubscribeReq{ServiceTypes: serviceTypes}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	pkt, err := c.call(context.Background(), CmdSubscribe, data)
	if err != nil {
		return nil, err
	}
	var res pb.SubscribeRes
	if err := proto.Unmarshal(pkt.Payload, &res); err != nil {
		return nil, err
	}
	return &res, nil
}

func (c *Client) sendWatch(serviceType string) error {
	req := &pb.ServiceType{ServiceType: serviceType}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  CmdWatch,
			SeqID:  0,
			Flags:  uint32(net.FlagRPCFF),
			Length: uint32(net.HeaderSize + len(data)),
		},
		Payload: data,
	}
	if !c.pool.SendPacket(pkt) {
		return fmt.Errorf("send watch request failed")
	}
	return nil
}

// call 同步 RPC 调用（Req-Res 配对）
func (c *Client) call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	seq := c.nextSeqID()
	ch := make(chan *net.Packet, 1)
	c.pending.Store(seq, ch)
	defer c.pending.Delete(seq)

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

	if !c.pool.SendPacket(pkt) {
		return nil, fmt.Errorf("no healthy registry node available")
	}

	ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()

	select {
	case res := <-ch:
		return res, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}

func (c *Client) nextSeqID() uint32 {
	c.seqMu.Lock()
	defer c.seqMu.Unlock()
	c.seqID++
	return c.seqID
}

func (c *Client) handlePacket(_ *net.TCPConn, pkt *net.Packet) {
	if pkt.CmdID == CmdNodeEvent {
		var ev pb.NodeEvent
		if err := proto.Unmarshal(pkt.Payload, &ev); err == nil && c.onEvent != nil {
			c.onEvent(&ev)
		}
		return
	}

	// RPC 响应分发
	if pkt.SeqID == 0 {
		return
	}
	if ch, ok := c.pending.Load(pkt.SeqID); ok {
		select {
		case ch.(chan *net.Packet) <- pkt:
		default:
		}
		c.pending.Delete(pkt.SeqID)
	}
}
