package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/config"
	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/registry"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/chat-go/internal/chat"
	commonpb "github.com/gmaker/luffa/gen/go/common"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	pb "github.com/gmaker/luffa/gen/go/registry"
)

// 命令 ID 定义 (来自 protocol.proto)
const (
	// Chat 服务命令 (0x00030000 - 0x0003FFFF)
	CmdChatCreateRoomReq = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_REQ)
	CmdChatCreateRoomRes = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_RES)
	CmdChatJoinRoomReq   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_REQ)
	CmdChatJoinRoomRes   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_RES)
	CmdChatLeaveRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_REQ)
	CmdChatLeaveRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_RES)
	CmdChatSendMsgReq    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_REQ)
	CmdChatSendMsgRes    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_RES)
	CmdChatMsgNotify     = uint32(protocol.CmdChat_CMD_CHAT_MSG_NOTIFY)
	CmdChatGetHistoryReq = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_REQ)
	CmdChatGetHistoryRes = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_RES)
	CmdChatCloseRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_REQ)
	CmdChatCloseRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_RES)

	// DBProxy 命令
	CmdDBQuery    = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	CmdDBQueryRes = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY_RES)
	CmdDBExec     = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
	CmdDBExecRes  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC_RES)

	// Gateway 内部命令
	CmdGWRoomJoin  = uint32(protocol.CmdGatewayInternal_CMD_GW_ROOM_JOIN)
	CmdGWRoomLeave = uint32(protocol.CmdGatewayInternal_CMD_GW_ROOM_LEAVE)
)

// ChatConfig Chat 服务配置
type ChatConfig struct {
	Service  ServiceConfig  `json:"service"`
	Network  NetworkConfig  `json:"network"`
	Registry RegistryConfig `json:"registry"`
	Redis    RedisConfig    `json:"redis"`
	DBProxy  DBProxyConfig  `json:"dbproxy"`
}

type ServiceConfig struct {
	ServiceType string `json:"service_type"`
	NodeID      string `json:"node_id"`
	LogLevel    string `json:"log_level"`
	LogFile     string `json:"log_file"`
	MetricsAddr string `json:"metrics_addr"`
}

type NetworkConfig struct {
	Host           string `json:"host"`
	Port           int    `json:"port"`
	MaxConnections int    `json:"max_connections"`
}

type RegistryConfig struct {
	Nodes []config.UpstreamNode `json:"nodes"`
}

type RedisConfig struct {
	Addrs    []string `json:"addrs"`
	Password string   `json:"password"`
	PoolSize int      `json:"pool_size"`
}

type DBProxyConfig struct {
	Nodes []config.UpstreamNode `json:"nodes"`
}

func main() {
	configFile := flag.String("config", "chat.json", "Config file path")
	flag.Parse()

	// 加载配置
	var cfg ChatConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}
	fmt.Printf("[Config] Loaded from %s\n", *configFile)

	// 打印配置
	fmt.Println("=== Chat Configuration ===")
	fmt.Printf("Node ID:     %s\n", cfg.Service.NodeID)
	fmt.Printf("Listen:      %s:%d\n", cfg.Network.Host, cfg.Network.Port)
	fmt.Printf("Metrics:     %s\n", cfg.Service.MetricsAddr)
	fmt.Printf("Log level:   %s\n", cfg.Service.LogLevel)
	fmt.Println("==========================")

	// 初始化日志
	log := logger.InitServiceLogger(cfg.Service.ServiceType, cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	// 启动 metrics
	metrics.ServeDefaultHTTP(cfg.Service.MetricsAddr)
	reqCounter := metrics.DefaultCounter("chat_requests_total")
	reqLatency := metrics.DefaultHistogram("chat_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("chat_connections")

	// 解析 node ID
	nodeIDParts := strings.Split(cfg.Service.NodeID, "-")
	var nodeID int64 = 1
	if len(nodeIDParts) > 1 {
		if n, err := fmt.Sscanf(nodeIDParts[len(nodeIDParts)-1], "%d", &nodeID); err != nil || n != 1 {
			nodeID = 1
		}
	}

	idGen, err := idgen.NewSnowflake(nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 连接 Registry
	var registryAddrs []string
	for _, n := range cfg.Registry.Nodes {
		registryAddrs = append(registryAddrs, fmt.Sprintf("%s:%d", n.Host, n.Port))
	}
	regClient := registry.NewClient(registryAddrs)
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	// 注册到 Registry
	node := &pb.NodeInfo{
		ServiceType: cfg.Service.ServiceType,
		NodeId:      cfg.Service.NodeID,
		Host:        cfg.Network.Host,
		Port:        uint32(cfg.Network.Port),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.RegisterWithRetry(node, 5); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Chat registered to registry")

	// 连接 Redis
	var redisClient *redis.Client
	if len(cfg.Redis.Addrs) > 0 {
		redisClient = redis.NewClient(redis.Config{
			Addrs:    cfg.Redis.Addrs,
			Password: cfg.Redis.Password,
			PoolSize: cfg.Redis.PoolSize,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, running without redis", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Chat connected to redis")
		}
	}

	// 通过 Registry 发现 DBProxy
	var chatSvc *chat.ChatService
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		// DBProxy 响应由 rpc.Client 处理
	}
	upstreamMgr := registry.NewUpstreamManager(regClient)
	upstreamMgr.AddInterest("dbproxy", dbproxyOnData)
	if err := upstreamMgr.Start(); err != nil {
		log.Warnf("subscribe dbproxy upstream failed: %v", err)
	}

	var dbClient chat.DBProxyClient
	dbproxyPool := upstreamMgr.GetPool("dbproxy")
	if dbproxyPool == nil || dbproxyPool.TotalCount() == 0 {
		log.Warnf("no dbproxy found in registry, using fallback config")
		var dbproxyAddrs []string
		for _, n := range cfg.DBProxy.Nodes {
			dbproxyAddrs = append(dbproxyAddrs, fmt.Sprintf("%s:%d", n.Host, n.Port))
		}
		dbClient = chat.NewDBProxyClient(dbproxyAddrs)
	} else {
		log.Infof("Chat discovered dbproxy from registry, nodes=%d", dbproxyPool.TotalCount())
		dbClient = chat.NewDBProxyClientWithPool(dbproxyPool)
	}
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
		dbClient = nil
	} else {
		log.Info("Chat connected to dbproxy")
	}

	chatSvc = chat.NewChatService(dbClient, redisClient, idGen, log)
	if dbClient != nil {
		if err := chat.EnsureChatRoomTable(chatSvc); err != nil {
			log.Warnf("ensure chat room table failed: %v, running without dbproxy", err)
			dbClient = nil
			chatSvc = chat.NewChatService(nil, redisClient, idGen, log)
		}
	}

	// 启动 TCP 服务器
	listenAddr := fmt.Sprintf("%s:%d", cfg.Network.Host, cfg.Network.Port)
	var srv *net.TCPServer
	srvCfg := net.ServerConfig{
		Addr: listenAddr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			start := time.Now()
			connGauge.Inc()
			reqCounter.Inc()
			log.Info(fmt.Sprintf("[Flow] OnData: cmd=0x%08X seq=%d", pkt.CmdID, pkt.SeqID))
			if chatSvc == nil {
				chat.SendProto(conn, pkt.SeqID, pkt.CmdID+1, &commonpb.Result{Ok: false, Code: 503, Msg: "service unavailable"})
			} else {
				chat.HandlePacket(conn, pkt, chatSvc, srv)
			}
			reqLatency.Observe(float64(time.Since(start).Milliseconds()))
			connGauge.Dec()
		},
		OnClose: func(conn *net.TCPConn) {
			log.Infof("Chat connection closed: %s", conn.ID())
		},
	}
	srv = net.NewTCPServer(srvCfg)
	if chatSvc != nil {
		chatSvc.SetServer(srv)
	}
	if err := srv.Start(); err != nil {
		log.Fatalf("start chat server failed: %v", err)
	}
	log.Infof("Chat server started on %s", listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down chat server...")
	srv.Stop()
}

func parsePort(addr string) uint32 {
	parts := strings.Split(addr, ":")
	if len(parts) >= 2 {
		var port int
		if _, err := fmt.Sscanf(parts[len(parts)-1], "%d", &port); err == nil {
			return uint32(port)
		}
	}
	return 0
}
