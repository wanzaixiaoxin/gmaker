package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/config"
	"github.com/gmaker/luffa/common/go/discovery"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/mysql"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/server"
)

type DBProxyConfig struct {
	Service struct {
		ServiceType string `json:"service_type"`
		NodeID      string `json:"node_id"`
		LogLevel    string `json:"log_level"`
		LogFile     string `json:"log_file"`
	} `json:"service"`
	Network struct {
		Host string `json:"host"`
		Port int    `json:"port"`
	} `json:"network"`
	Discovery struct {
		Type  string   `json:"type"`
		Addrs []string `json:"addrs"`
	} `json:"discovery"`
}

func main() {
	var (
		configFile    = flag.String("config", "dbproxy.json", "Config file path")
		listenAddr    = flag.String("listen", "", "DBProxy listen address (overrides config)")
		mysqlDSNs     = flag.String("mysql", "", "MySQL DSNs, comma separated")
		logFile       = flag.String("log-file", "", "Log file path (overrides config)")
		logLevel      = flag.String("log-level", "", "Log level (overrides config)")
	)
	flag.Parse()

	// 加载配置文件
	var cfg DBProxyConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	// 命令行参数覆盖配置文件
	listen := fmt.Sprintf("%s:%d", cfg.Network.Host, cfg.Network.Port)
	if *listenAddr != "" {
		listen = *listenAddr
	}
	if *logFile == "" {
		*logFile = cfg.Service.LogFile
	}
	if *logLevel == "" {
		*logLevel = cfg.Service.LogLevel
	}

	log := logger.InitServiceLogger("dbproxy", cfg.Service.NodeID, *logLevel, *logFile)
	logger.SetDefault(log)

	// MySQL
	var mproxy *mysql.Proxy
	if *mysqlDSNs != "" {
		var cfgs []mysql.Config
		for _, dsn := range strings.Split(*mysqlDSNs, ",") {
			cfgs = append(cfgs, mysql.Config{
				DSN:         dsn,
				MaxOpenConn: 20,
				MaxIdleConn: 5,
			})
		}
		var err error
		mproxy, err = mysql.NewProxy(cfgs)
		if err != nil {
			log.Fatalf("mysql proxy init failed: %v", err)
		}
		defer mproxy.Close()
	}

	// 初始化服务发现（从配置读取）
	sd, err := discovery.New(cfg.Discovery.Type, cfg.Discovery.Addrs)
	if err != nil {
		log.Fatalf("init discovery failed: %v", err)
	}
	defer sd.Close()

	node := discovery.NodeInfo{
		ServiceType: "dbproxy",
		NodeID:      cfg.Service.NodeID,
		Host:        cfg.Network.Host,
		Port:        uint32(cfg.Network.Port),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if err := sd.Register(nil, node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Infof("DBProxy registered to %s", cfg.Discovery.Type)

	srv := server.New(listen, mproxy)
	if err := srv.Start(); err != nil {
		log.Fatalf("start dbproxy failed: %v", err)
	}
	log.Infof("DBProxy started on %s", listen)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down dbproxy...")
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
