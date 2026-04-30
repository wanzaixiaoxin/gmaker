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
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/services/bot-go/internal/api"
	"github.com/gmaker/luffa/services/bot-go/internal/bot"
	"github.com/gmaker/luffa/services/bot-go/internal/dbproxy"
)

// BotConfig bot 服务配置
type BotConfig struct {
	Service struct {
		ServiceType string `json:"service_type"`
		NodeID      string `json:"node_id"`
		LogLevel    string `json:"log_level"`
		LogFile     string `json:"log_file"`
		AdminPort   int    `json:"admin_port"`
	} `json:"service"`
	Gateway struct {
		Addr      string `json:"addr"`
		MasterKey string `json:"master_key"`
	} `json:"gateway"`
	DBProxy struct {
		Nodes []config.UpstreamNode `json:"nodes"`
	} `json:"dbproxy"`
	Bots bot.Config `json:"bots"`
}

func main() {
	configFile := flag.String("config", "conf/bot.json", "Config file path")
	flag.Parse()

	var cfg BotConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	log := logger.InitServiceLogger(cfg.Service.ServiceType, cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	if cfg.Gateway.MasterKey == "" {
		log.Fatal("gateway.master_key is required for bot authentication")
	}

	// 解析 node_id（如 "bot-1"）中的数字作为 Snowflake node ID
	var nodeID int64 = 999
	parts := strings.Split(cfg.Service.NodeID, "-")
	if len(parts) > 1 {
		fmt.Sscanf(parts[len(parts)-1], "%d", &nodeID)
	}
	if nodeID == 0 {
		nodeID = 999
	}
	idGen, err := idgen.NewSnowflake(nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 初始化服务发现（用于发现 DBProxy）
	var sd discovery.ServiceDiscovery
	if len(cfg.DBProxy.Nodes) == 0 {
		var err error
		sd, err = discovery.New("memory", nil)
		if err != nil {
			log.Warnf("init discovery failed: %v", err)
		}
	}
	if sd != nil {
		defer sd.Close()
	}

	// 初始化 DBProxy 连接
	var dbClient *dbproxy.Client
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		if dbClient != nil {
			dbClient.OnPacket(pkt)
		}
	}

	var dbproxyPool *net.UpstreamPool
	if sd != nil {
		upstreamMgr := discovery.NewUpstreamManager(sd)
		upstreamMgr.AddInterest("dbproxy", dbproxyOnData)
		if err := upstreamMgr.Start(); err != nil {
			log.Warnf("subscribe dbproxy upstream failed: %v", err)
		}
		dbproxyPool = upstreamMgr.GetPool("dbproxy")
	}

	if dbproxyPool == nil || dbproxyPool.TotalCount() == 0 {
		log.Warnf("no dbproxy found in registry, using fallback config")
		dbproxyPool = net.NewUpstreamPool(dbproxyOnData)
		for _, n := range cfg.DBProxy.Nodes {
			dbproxyPool.AddNode(fmt.Sprintf("%s:%d", n.Host, n.Port))
		}
		dbproxyPool.Start()
	}

	dbClient = dbproxy.NewClient(dbproxyPool)
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
		dbClient = nil
	} else {
		log.Info("Bot connected to dbproxy")
	}

	// 创建 Bot 管理器
	mgr := bot.NewManager(cfg.Bots, cfg.Gateway.Addr, cfg.Gateway.MasterKey, dbClient, idGen, log)

	// 若配置为自动启动，则立即启动
	if cfg.Bots.AutoStart {
		mgr.Start()
	}

	// 启动 HTTP 管理 API
	mux := http.NewServeMux()
	apiServer := api.NewServer(mgr, log)
	apiServer.RegisterRoutes(mux)

	adminAddr := fmt.Sprintf(":%d", cfg.Service.AdminPort)
	httpServer := &http.Server{Addr: adminAddr, Handler: mux}

	go func() {
		log.Infof("Bot admin HTTP server started on %s", adminAddr)
		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("admin server failed: %v", err)
		}
	}()

	// 等待退出信号
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down bot service...")
	mgr.Stop()

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_ = httpServer.Shutdown(ctx)
	if dbClient != nil {
		dbClient.Close()
	}
}
