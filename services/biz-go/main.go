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
	"github.com/gmaker/luffa/common/go/config"
	"github.com/gmaker/luffa/common/go/discovery"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/biz-go/internal/dbproxy"
	"github.com/gmaker/luffa/services/biz-go/internal/handler"
	"github.com/gmaker/luffa/services/biz-go/internal/service"

	commonpb "github.com/gmaker/luffa/gen/go/common"
)

type BizConfig struct {
	Service struct {
		ServiceType string `json:"service_type"`
		NodeID      string `json:"node_id"`
		LogLevel    string `json:"log_level"`
		LogFile     string `json:"log_file"`
		MetricsAddr string `json:"metrics_addr"`
	} `json:"service"`
	Network struct {
		Host string `json:"host"`
		Port int    `json:"port"`
	} `json:"network"`
	Discovery struct {
		Type  string   `json:"type"`
		Addrs []string `json:"addrs"`
	} `json:"discovery"`
	Redis struct {
		Addrs    []string `json:"addrs"`
		Password string   `json:"password"`
		PoolSize int      `json:"pool_size"`
	} `json:"redis"`
	Bot struct {
		MasterKey string `json:"master_key"`
	} `json:"bot"`
}

func main() {
	var (
		configFile    = flag.String("config", "conf/biz.json", "Config file path")
		dbproxyAddrs  = flag.String("dbproxy", "", "DBProxy addresses, comma separated (overrides config)")
		redisAddrs    = flag.String("redis", "", "Redis addresses, comma separated (overrides config)")
		redisPass     = flag.String("redis-pass", "", "Redis password")
	)
	flag.Parse()

	// 加载配置文件
	var cfg BizConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	// 从 node_id（如 "biz-1"）解析 Snowflake 数字节点 ID
	var nodeID int64 = 1
	parts := strings.Split(cfg.Service.NodeID, "-")
	if len(parts) > 1 {
		fmt.Sscanf(parts[len(parts)-1], "%d", &nodeID)
	}
	if nodeID == 0 {
		nodeID = 1
	}

	host := cfg.Network.Host
	port := cfg.Network.Port
	listen := fmt.Sprintf("%s:%d", host, port)

	// 初始化结构化日志
	log := logger.InitServiceLogger("biz", cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	// 启动 Prometheus metrics HTTP 服务
	metrics.ServeDefaultHTTP(cfg.Service.MetricsAddr)
	reqCounter := metrics.DefaultCounter("biz_requests_total")
	reqLatency := metrics.DefaultHistogram("biz_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("biz_connections")

	// 初始化 Snowflake ID 生成器
	idGen, err := idgen.NewSnowflake(nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 初始化服务发现（Registry 或 etcd，从配置读取）
	sd, err := discovery.New(cfg.Discovery.Type, cfg.Discovery.Addrs)
	if err != nil {
		log.Fatalf("init discovery failed: %v", err)
	}
	defer sd.Close()

	node := discovery.NodeInfo{
		ServiceType: "biz",
		NodeID:      cfg.Service.NodeID,
		Host:        host,
		Port:        uint32(port),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if err := sd.Register(context.Background(), node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Infof("Biz registered to %s", cfg.Discovery.Type)

	// 初始化 Redis 客户端（优先配置文件，其次 CLI 参数覆盖）
	var redisClient *redis.Client
	var redisAddrsList []string
	if len(cfg.Redis.Addrs) > 0 {
		redisAddrsList = cfg.Redis.Addrs
	} else if *redisAddrs != "" {
		redisAddrsList = strings.Split(*redisAddrs, ",")
	}
	redisPassword := cfg.Redis.Password
	if redisPassword == "" && *redisPass != "" {
		redisPassword = *redisPass
	}
	if len(redisAddrsList) > 0 {
		poolSize := cfg.Redis.PoolSize
		if poolSize <= 0 { poolSize = 20 }
		redisClient = redis.NewClient(redis.Config{
			Addrs:    redisAddrsList,
			Password: redisPassword,
			PoolSize: poolSize,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, running without redis", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Biz connected to redis")
		}
	}

	// 通过服务发现订阅 DBProxy
	var playerSvc *service.PlayerService
	var dbClient *dbproxy.Client
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		if dbClient != nil {
			dbClient.OnPacket(pkt)
		}
	}
	upstreamMgr := discovery.NewUpstreamManager(sd)
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

	dbClient = dbproxy.NewClient(dbproxyPool)
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
	} else {
		log.Info("Biz connected to dbproxy")
		playerSvc = service.NewPlayerService(dbClient, redisClient, idGen, log, cfg.Bot.MasterKey)
		if err := service.EnsurePlayerProfileTable(playerSvc); err != nil {
			log.Warnf("ensure player profile table failed: %v, player service disabled", err)
		}
	}

	// 启动 TCP 服务
	srvCfg := net.ServerConfig{
		Addr: listen,
		OnConnect: func(conn *net.TCPConn) {
			connGauge.Inc()
			log.Infof("Biz connection opened: %d", conn.ID())
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
			log.Infof("Biz connection closed: %d", conn.ID())
		},
	}
	srv := net.NewTCPServer(srvCfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start biz server failed: %v", err)
	}
	log.Infof("Biz server started on %s", listen)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down biz server...")
	srv.Stop()
}

func parseHostPort(addr string) (string, int) {
	parts := strings.Split(addr, ":")
	if len(parts) >= 2 {
		host := strings.Join(parts[:len(parts)-1], ":")
		if port, err := strconv.Atoi(parts[len(parts)-1]); err == nil {
			return host, port
		}
	}
	return "127.0.0.1", 8082
}

func parsePort(addr string) uint32 {
	_, port := parseHostPort(addr)
	return uint32(port)
}
