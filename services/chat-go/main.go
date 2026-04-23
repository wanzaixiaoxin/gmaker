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

const (
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

	CmdMySQLExec    = uint32(0x000E0013)
	CmdMySQLExecRes = uint32(0x000E0014)
	CmdMySQLQuery   = uint32(0x000E0011)
	CmdMySQLQueryRes = uint32(0x000E0012)

	CmdGWRoomJoin  = uint32(0x00001010)
	CmdGWRoomLeave = uint32(0x00001011)
)

func main() {
	var (
		listenAddr    = flag.String("listen", ":8086", "Chat listen address")
		registryAddrs = flag.String("registry", "127.0.0.1:2379", "Registry addresses")
		dbproxyAddrs  = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy addresses")
		redisAddrs    = flag.String("redis", "127.0.0.1:6379", "Redis addresses")
		redisPass     = flag.String("redis-pass", "", "Redis password")
		nodeID        = flag.Int64("node-id", 1, "Snowflake node ID")
		metricsAddr   = flag.String("metrics", ":9086", "Metrics HTTP address")
		logFile       = flag.String("log-file", "", "Log file path")
		logLevel      = flag.String("log-level", "info", "Log level")
	)
	flag.Parse()

	log := logger.InitServiceLogger("chat", fmt.Sprintf("chat-%d", *nodeID), *logLevel, *logFile)
	logger.SetDefault(log)

	metrics.ServeDefaultHTTP(*metricsAddr)
	reqCounter := metrics.DefaultCounter("chat_requests_total")
	reqLatency := metrics.DefaultHistogram("chat_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("chat_connections")

	idGen, err := idgen.NewSnowflake(*nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	regClient := registry.NewClient(strings.Split(*registryAddrs, ","))
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "chat",
		NodeId:      fmt.Sprintf("chat-%d", *nodeID),
		Host:        "127.0.0.1",
		Port:        parsePort(*listenAddr),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.RegisterWithRetry(node, 5); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Chat registered to registry")

	var redisClient *redis.Client
	if *redisAddrs != "" {
		redisClient = redis.NewClient(redis.Config{
			Addrs:    strings.Split(*redisAddrs, ","),
			Password: *redisPass,
			PoolSize: 20,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, running without redis", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Chat connected to redis")
		}
	}

	var chatSvc *chat.ChatService
	dbClient := chat.NewDBProxyClient(strings.Split(*dbproxyAddrs, ","))
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

	var srv *net.TCPServer
	cfg := net.ServerConfig{
		Addr: *listenAddr,
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
	srv = net.NewTCPServer(cfg)
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
