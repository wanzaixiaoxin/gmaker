package main

import (
	"context"
	"encoding/binary"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/config"
	"github.com/gmaker/luffa/common/go/discovery"
	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/redis"

	"github.com/gmaker/luffa/services/match-go/internal/handler"
	"github.com/gmaker/luffa/services/match-go/internal/service"
	"github.com/gmaker/luffa/services/match-go/internal/store"
	commonpb "github.com/gmaker/luffa/gen/go/common"

	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/concurrency"
)

type MatchConfig struct {
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
	Election struct {
		Enabled  bool   `json:"enabled"`
		EtcdAddr string `json:"etcd_addr"`
		LockKey  string `json:"lock_key"`
	} `json:"election"`
}

func main() {
	var (
		configFile = flag.String("config", "conf/match.json", "Config file path")
		redisAddrs = flag.String("redis", "", "Redis addresses, comma separated (overrides config)")
		redisPass  = flag.String("redis-pass", "", "Redis password")
	)
	flag.Parse()

	// 加载配置
	var cfg MatchConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	// 解析 nodeID
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

	// 初始化日志
	log := logger.InitServiceLogger("match", cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	// Prometheus metrics
	metrics.ServeDefaultHTTP(cfg.Service.MetricsAddr)
	reqCounter := metrics.DefaultCounter("match_requests_total")
	reqLatency := metrics.DefaultHistogram("match_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("match_connections")

	// Snowflake ID
	idGen, err := idgen.NewSnowflake(nodeID)
	_ = idGen
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 服务发现
	sd, err := discovery.New(cfg.Discovery.Type, cfg.Discovery.Addrs)
	if err != nil {
		log.Fatalf("init discovery failed: %v", err)
	}
	defer sd.Close()

	node := discovery.NodeInfo{
		ServiceType: "match",
		NodeID:      cfg.Service.NodeID,
		Host:        host,
		Port:        uint32(port),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if err := sd.Register(context.Background(), node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Infof("Match registered to %s", cfg.Discovery.Type)

	// Redis 客户端
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
		if poolSize <= 0 {
			poolSize = 20
		}
		redisClient = redis.NewClient(redis.Config{
			Addrs:    redisAddrsList,
			Password: redisPassword,
			PoolSize: poolSize,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, running without redis checkpoint", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Match connected to redis")
		}
	}

	// 构建 RedisStore（nil safe）
	var redisStore *store.RedisStore
	if redisClient != nil {
		redisStore = store.NewRedisStore(redisClient.RawClient())
	} else {
		redisStore = store.NewRedisStore(nil)
	}

	// 初始化 MatchService
	memStore := store.NewMemoryStore()
	matchSvc := service.NewMatchService(memStore, redisStore, log)

	// 订阅 Realtime 上游
	realtimeOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		// TODO: 处理 Realtime 的房间创建响应
		log.Infof("[Realtime] received response: cmd=0x%08X", pkt.CmdID)
	}
	upstreamMgr := discovery.NewUpstreamManager(sd)
	upstreamMgr.AddInterest("realtime", realtimeOnData)
	if err := upstreamMgr.Start(); err != nil {
		log.Warnf("subscribe realtime upstream failed: %v", err)
	}
	realtimePool := upstreamMgr.GetPool("realtime")
	if realtimePool != nil {
		matchSvc.SetRealtimePool(realtimePool)
		log.Infof("Match discovered realtime from registry")
	}

	// 崩溃恢复
	if err := matchSvc.Recover(); err != nil {
		log.Warnf("recovery failed: %v", err)
	}

	// etcd 选举（可选）
	if cfg.Election.Enabled && cfg.Election.EtcdAddr != "" {
		go runElection(cfg.Election.EtcdAddr, cfg.Election.LockKey, cfg.Service.NodeID, matchSvc, log)
	} else {
		// 无选举时默认为主节点
		matchSvc.SetLeader(true)
		log.Info("Match running as leader (no election)")
	}

	// 启动 TCP 服务
	srvCfg := net.ServerConfig{
		Addr: listen,
		OnConnect: func(conn *net.TCPConn) {
			connGauge.Inc()
			log.Infof("Match connection opened: %d", conn.ID())
		},
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			start := time.Now()
			reqCounter.Inc()
			if !matchSvc.IsLeader() {
				// 非主节点返回服务不可用
				var gatewayConnID uint64
				if len(pkt.Payload) >= 8 {
					gatewayConnID = binary.BigEndian.Uint64(pkt.Payload[:8])
				}
				handler.SendProto(conn, pkt.SeqID, pkt.CmdID+1,
					&commonpb.Result{Ok: false, Code: 503, Msg: "not leader"}, gatewayConnID)
			} else {
				handler.HandleMatchPacket(conn, pkt, matchSvc)
			}
			reqLatency.Observe(float64(time.Since(start).Milliseconds()))
		},
		OnClose: func(conn *net.TCPConn) {
			connGauge.Dec()
			log.Infof("Match connection closed: %d", conn.ID())
		},
	}
	srv := net.NewTCPServer(srvCfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start match server failed: %v", err)
	}
	log.Infof("Match server started on %s", listen)

	// 优雅关闭
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down match server...")
	matchSvc.SetLeader(false)
	srv.Stop()
}

// runElection etcd leader 选举
func runElection(etcdAddr, lockKey, nodeID string, svc *service.MatchService, log *logger.Logger) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   strings.Split(etcdAddr, ","),
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		log.Errorf("connect etcd for election failed: %v, running as leader", err)
		svc.SetLeader(true)
		return
	}
	defer cli.Close()

	session, err := concurrency.NewSession(cli)
	if err != nil {
		log.Errorf("create etcd session failed: %v, running as leader", err)
		svc.SetLeader(true)
		return
	}
	defer session.Close()

	elector := concurrency.NewElection(session, lockKey)

	for {
		ctx := context.Background()
		log.Infof("[Election] campaigning for leader: node=%s key=%s", nodeID, lockKey)
		if err := elector.Campaign(ctx, nodeID); err != nil {
			log.Errorf("[Election] campaign failed: %v, retrying in 5s...", err)
			time.Sleep(5 * time.Second)
			continue
		}

		log.Infof("[Election] became leader: node=%s", nodeID)
		svc.SetLeader(true)

		// 监听 session 是否失效
		select {
		case <-session.Done():
			log.Warn("[Election] session lost, stepping down")
			svc.SetLeader(false)
			// 重新竞选
			continue
		case <-ctx.Done():
			return
		}
	}
}

// unused import guard
var _ sync.WaitGroup
