package main

import (
	"context"
	"encoding/binary"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/registry"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/biz-go/internal/dbproxy"
	"github.com/gmaker/luffa/services/biz-go/internal/handler"
	"github.com/gmaker/luffa/services/biz-go/internal/service"

	commonpb "github.com/gmaker/luffa/gen/go/common"
	pb "github.com/gmaker/luffa/gen/go/registry"
)

func main() {
	var (
		listenAddr    = flag.String("listen", ":8082", "Biz listen address")
		registryAddrs = flag.String("registry", "127.0.0.1:2379", "Registry addresses, comma separated")
		dbproxyAddrs  = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy addresses, comma separated")
		redisAddrs    = flag.String("redis", "127.0.0.1:6379", "Redis addresses, comma separated")
		redisPass     = flag.String("redis-pass", "", "Redis password")
		nodeID        = flag.Int64("node-id", 1, "Snowflake node ID")
		metricsAddr   = flag.String("metrics", ":9082", "Metrics HTTP address")
		logFile       = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel      = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	// 初始化结构化日志
	log := logger.InitServiceLogger("biz", fmt.Sprintf("biz-%d", *nodeID), *logLevel, *logFile)
	logger.SetDefault(log)

	// 启动 Prometheus metrics HTTP 服务
	metrics.ServeDefaultHTTP(*metricsAddr)
	reqCounter := metrics.DefaultCounter("biz_requests_total")
	reqLatency := metrics.DefaultHistogram("biz_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("biz_connections")

	// 初始化 Snowflake ID 生成器
	idGen, err := idgen.NewSnowflake(*nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 注册到 Registry（多节点）
	regClient := registry.NewClient(strings.Split(*registryAddrs, ","))
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "biz",
		NodeId:      fmt.Sprintf("biz-%d", *nodeID),
		Host:        "127.0.0.1",
		Port:        parsePort(*listenAddr),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.RegisterWithRetry(node, 5); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Biz registered to registry")

	// 启动 Registry 心跳保活
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				_, err := regClient.Heartbeat(node.NodeId)
				if err != nil {
					log.Warnf("registry heartbeat failed: %v", err)
				}
			}
		}
	}()

	// 初始化 Redis 客户端（直接连接，不经过 DBProxy）
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
			log.Info("Biz connected to redis")
		}
	}

	// 通过 Registry 发现 DBProxy
	var playerSvc *service.PlayerService
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		// DBProxy 响应由 rpc.Client 处理，此回调在 NewDBProxyClient 中通过 pool 绑定
	}
	upstreamMgr := registry.NewUpstreamManager(regClient)
	upstreamMgr.AddInterest("dbproxy", dbproxyOnData)
	if err := upstreamMgr.Start(); err != nil {
		log.Warnf("subscribe dbproxy upstream failed: %v", err)
	}

	dbproxyPool := upstreamMgr.GetPool("dbproxy")
	if dbproxyPool == nil || dbproxyPool.TotalCount() == 0 {
		log.Warnf("no dbproxy found in registry, using fallback addrs: %s", *dbproxyAddrs)
		dbproxyPool = net.NewUpstreamPool(dbproxyOnData)
		for _, addr := range strings.Split(*dbproxyAddrs, ",") {
			dbproxyPool.AddNode(addr)
		}
		dbproxyPool.Start()
	} else {
		log.Infof("Biz discovered dbproxy from registry, nodes=%d", dbproxyPool.TotalCount())
	}

	dbClient := dbproxy.NewClient(dbproxyPool)
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
	} else {
		log.Info("Biz connected to dbproxy")
		playerSvc = service.NewPlayerService(dbClient, redisClient, idGen, log)
		if err := service.EnsurePlayerTable(playerSvc); err != nil {
			log.Warnf("ensure player table failed: %v, player service disabled", err)
		}
	}

	// 启动 TCP 服务
	cfg := net.ServerConfig{
		Addr: *listenAddr,
		OnConnect: func(conn *net.TCPConn) {
			connGauge.Inc()
			log.Infof("Biz connection opened: %s", conn.ID())
		},
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			start := time.Now()
			reqCounter.Inc()
			log.Info(fmt.Sprintf("[Flow] OnData: cmd=0x%08X seq=%d", pkt.CmdID, pkt.SeqID))
			if playerSvc == nil {
				// DBProxy 不可用时返回服务不可用
				var gatewayConnID uint64
				if len(pkt.Payload) >= 8 {
					gatewayConnID = binary.BigEndian.Uint64(pkt.Payload[:8])
				}
				handler.SendProto(conn, pkt.SeqID, pkt.CmdID+1, &commonpb.Result{Ok: false, Code: 503, Msg: "service unavailable"}, gatewayConnID)
			} else {
				handler.HandleBizPacket(conn, pkt, playerSvc)
			}
			reqLatency.Observe(float64(time.Since(start).Milliseconds()))
		},
		OnClose: func(conn *net.TCPConn) {
			connGauge.Dec()
			log.Infof("Biz connection closed: %s", conn.ID())
		},
	}
	srv := net.NewTCPServer(cfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start biz server failed: %v", err)
	}
	log.Infof("Biz server started on %s", *listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down biz server...")
	srv.Stop()
}

func parsePort(addr string) uint32 {
	parts := strings.Split(addr, ":")
	if len(parts) >= 2 {
		if p, err := strconv.ParseUint(parts[len(parts)-1], 10, 32); err == nil {
			return uint32(p)
		}
	}
	return 0
}
