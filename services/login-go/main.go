package main

import (
	"context"
	"flag"
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/config"
	"github.com/gmaker/luffa/common/go/discovery"
	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/login-go/internal/dbproxy"
	"github.com/gmaker/luffa/services/login-go/internal/handler"
	"github.com/gmaker/luffa/services/login-go/internal/service"
)

// LoginConfig Login 服务配置
type LoginConfig struct {
	Service   ServiceConfig   `json:"service"`
	Network   NetworkConfig   `json:"network"`
	Discovery DiscoveryConfig `json:"discovery"`
	Redis     RedisConfig     `json:"redis"`
	DBProxy   DBProxyConfig   `json:"dbproxy"`
}

type ServiceConfig struct {
	ServiceType string `json:"service_type"`
	NodeID      string `json:"node_id"`
	LogLevel    string `json:"log_level"`
	LogFile     string `json:"log_file"`
	MetricsAddr string `json:"metrics_addr"`
}

type NetworkConfig struct {
	Host string `json:"host"`
	Port int    `json:"port"`
}

type DiscoveryConfig struct {
	Type  string   `json:"type"`
	Addrs []string `json:"addrs"`
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
	configFile := flag.String("config", "conf/login.json", "Config file path")
	flag.Parse()

	var cfg LoginConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}
	fmt.Printf("[Config] Loaded from %s\n", *configFile)

	log := logger.InitServiceLogger(cfg.Service.ServiceType, cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	metrics.ServeDefaultHTTP(cfg.Service.MetricsAddr)

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

	// 初始化服务发现
	sd, err := discovery.New(cfg.Discovery.Type, cfg.Discovery.Addrs)
	if err != nil {
		log.Fatalf("init discovery failed: %v", err)
	}
	defer sd.Close()

	node := discovery.NodeInfo{
		ServiceType: cfg.Service.ServiceType,
		NodeID:      cfg.Service.NodeID,
		Host:        cfg.Network.Host,
		Port:        uint32(cfg.Network.Port),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if err := sd.Register(nil, node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Login registered to registry")

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
			log.Info("Login connected to redis")
		}
	}

	// 通过 Registry 发现 DBProxy
	var dbClient dbproxy.Client
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		if dbClient != nil {
			if c, ok := dbClient.(interface{ OnPacket(*net.Packet) }); ok {
				c.OnPacket(pkt)
			}
		}
	}
	upstreamMgr := discovery.NewUpstreamManager(sd)
	upstreamMgr.AddInterest("dbproxy", dbproxyOnData)
	if err := upstreamMgr.Start(); err != nil {
		log.Warnf("subscribe dbproxy upstream failed: %v", err)
	}

	dbproxyPool := upstreamMgr.GetPool("dbproxy")
	if dbproxyPool == nil || dbproxyPool.TotalCount() == 0 {
		log.Warnf("no dbproxy found in registry, using fallback config")
		var dbproxyAddrs []string
		for _, n := range cfg.DBProxy.Nodes {
			dbproxyAddrs = append(dbproxyAddrs, fmt.Sprintf("%s:%d", n.Host, n.Port))
		}
		dbClient = dbproxy.NewClient(dbproxyAddrs)
	} else {
		log.Infof("Login discovered dbproxy from registry, nodes=%d", dbproxyPool.TotalCount())
		dbClient = dbproxy.NewClientWithPool(dbproxyPool)
	}
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
		dbClient = nil
	} else {
		log.Info("Login connected to dbproxy")
	}

	playerSvc := service.NewPlayerService(dbClient, redisClient, idGen)
	if dbClient != nil {
		if err := service.EnsurePlayerTable(playerSvc); err != nil {
			log.Warnf("ensure player table failed: %v", err)
		} else {
			log.Info("Login ensured players table")
		}
	}

	// 发现 Gateway 地址
	gatewayAddr := discoverGatewayAddr(sd, cfg.Network.Host, cfg.Network.Port, log)

	// HTTP handler
	h := &handler.LoginHandler{
		PlayerSvc:   playerSvc,
		GatewayAddr: gatewayAddr,
		Log:         log,
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/api/v1/register", h.HandleRegister)
	mux.HandleFunc("/api/v1/login", h.HandleLogin)
	mux.HandleFunc("/api/v1/verify_token", h.HandleVerifyToken)

	listenAddr := fmt.Sprintf("%s:%d", cfg.Network.Host, cfg.Network.Port)
	server := &http.Server{Addr: listenAddr, Handler: mux}

	go func() {
		log.Infof("Login HTTP server started on %s", listenAddr)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("login server failed: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down login server...")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	server.Shutdown(ctx)
	if dbClient != nil {
		dbClient.Close()
	}
}

func discoverGatewayAddr(sd discovery.ServiceDiscovery, fallbackHost string, fallbackPort int, log *logger.Logger) string {
	// 从 Registry 查找 gateway 节点（Web 客户端需要 WebSocket 地址）
	nodes, err := sd.Discover(context.Background(), "gateway")
	if err == nil && len(nodes) > 0 {
		node := nodes[0]
		host := node.Host
		if host == "0.0.0.0" {
			host = "127.0.0.1"
		}
		// Gateway 注册的是 TCP 端口，WebSocket 端口固定 +2（8081->8083）
		wsPort := node.Port + 2
		addr := fmt.Sprintf("%s:%d", host, wsPort)
		log.Infof("Discovered gateway ws: %s", addr)
		return addr
	}
	// Fallback
	log.Warnf("no gateway found in registry, using fallback")
	host := fallbackHost
	if host == "0.0.0.0" {
		host = "127.0.0.1"
	}
	return fmt.Sprintf("%s:%d", host, 8083)
}
