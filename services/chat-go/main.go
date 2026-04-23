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
	pb "github.com/gmaker/luffa/gen/go/registry"
)

// 命令 ID 定义 (来自 protocol.proto)
const (
	// Chat 服务命令 (0x00030000 - 0x0003FFFF)
	CmdChatCreateRoomReq = uint32(0x00030000)
	CmdChatCreateRoomRes = uint32(0x00030001)
	CmdChatJoinRoomReq   = uint32(0x00030002)
	CmdChatJoinRoomRes   = uint32(0x00030003)
	CmdChatLeaveRoomReq  = uint32(0x00030004)
	CmdChatLeaveRoomRes  = uint32(0x00030005)
	CmdChatSendMsgReq    = uint32(0x00030006)
	CmdChatSendMsgRes    = uint32(0x00030007)
	CmdChatMsgNotify     = uint32(0x00030008)
	CmdChatGetHistoryReq = uint32(0x00030009)
	CmdChatGetHistoryRes = uint32(0x0003000A)
	CmdChatCloseRoomReq  = uint32(0x0003000B)
	CmdChatCloseRoomRes  = uint32(0x0003000C)

	// DBProxy 命令 (0x00002000 - 0x00002FFF)
	CmdDBQuery    = uint32(0x00002000)
	CmdDBQueryRes = uint32(0x00002001)
	CmdDBExec     = uint32(0x00002002)
	CmdDBExecRes  = uint32(0x00002003)

	// Gateway 内部命令 (0x00000100 - 0x000001FF)
	CmdGWRoomJoin  = uint32(0x00000100)
	CmdGWRoomLeave = uint32(0x00000101)
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

	// 连接 DBProxy
	var chatSvc *chat.ChatService
	var dbproxyAddrs []string
	for _, n := range cfg.DBProxy.Nodes {
		dbproxyAddrs = append(dbproxyAddrs, fmt.Sprintf("%s:%d", n.Host, n.Port))
	}
	dbClient := chat.NewDBProxyClient(dbproxyAddrs)
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
	log.Infof("Chat server started on %s", *listenAddr)

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
