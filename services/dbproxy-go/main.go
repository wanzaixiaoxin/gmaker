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
	MySQL struct {
		DSN             string `json:"dsn"`
		MaxOpenConn     int    `json:"max_open_conn"`
		MaxIdleConn     int    `json:"max_idle_conn"`
		ConnMaxLifetime int    `json:"conn_max_lifetime_sec"`
	} `json:"mysql"`
}

func main() {
	var (
		configFile    = flag.String("config", "conf/dbproxy.json", "Config file path")
		mysqlDSNs     = flag.String("mysql", "", "MySQL DSNs, comma separated (overrides config)")
	)
	flag.Parse()

	// 加载配置文件
	var cfg DBProxyConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	listen := fmt.Sprintf("%s:%d", cfg.Network.Host, cfg.Network.Port)

	log := logger.InitServiceLogger("dbproxy", cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	// MySQL
	var mproxy *mysql.Proxy
	dsnList := *mysqlDSNs
	if dsnList == "" && cfg.MySQL.DSN != "" {
		dsnList = cfg.MySQL.DSN
	}
	if dsnList != "" {
		var cfgs []mysql.Config
		for _, dsn := range strings.Split(dsnList, ",") {
			mCfg := mysql.Config{DSN: dsn}
			if cfg.MySQL.MaxOpenConn > 0 {
				mCfg.MaxOpenConn = cfg.MySQL.MaxOpenConn
			}
			if cfg.MySQL.MaxIdleConn > 0 {
				mCfg.MaxIdleConn = cfg.MySQL.MaxIdleConn
			}
			if cfg.MySQL.ConnMaxLifetime > 0 {
				mCfg.ConnMaxLifetime = time.Duration(cfg.MySQL.ConnMaxLifetime) * time.Second
			}
			cfgs = append(cfgs, mCfg)
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
