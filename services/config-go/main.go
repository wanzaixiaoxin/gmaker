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
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/config-go/internal/handler"
	"github.com/gmaker/luffa/services/config-go/internal/store"
)

type ConfigServiceConfig struct {
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
	MySQL struct {
		DSN             string `json:"dsn"`
		MaxOpenConn     int    `json:"max_open_conn"`
		MaxIdleConn     int    `json:"max_idle_conn"`
		ConnMaxLifetime int    `json:"conn_max_lifetime_sec"`
	} `json:"mysql"`
	Redis struct {
		Addrs    []string `json:"addrs"`
		Password string   `json:"password"`
		PoolSize int      `json:"pool_size"`
	} `json:"redis"`
}

func main() {
	var (
		configFile = flag.String("config", "conf/config.json", "Config file path")
		mysqlDSN   = flag.String("mysql", "", "MySQL DSN (overrides config)")
		redisAddrs = flag.String("redis", "", "Redis addresses, comma separated (overrides config)")
	)
	flag.Parse()

	var cfg ConfigServiceConfig
	if err := config.LoadJSON(*configFile, &cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Config] Failed to load %s: %v\n", *configFile, err)
		os.Exit(1)
	}

	log := logger.InitServiceLogger(cfg.Service.ServiceType, cfg.Service.NodeID, cfg.Service.LogLevel, cfg.Service.LogFile)
	logger.SetDefault(log)

	metrics.ServeDefaultHTTP(cfg.Service.MetricsAddr)

	// MySQL
	dsn := cfg.MySQL.DSN
	if *mysqlDSN != "" {
		dsn = *mysqlDSN
	}
	if dsn == "" {
		log.Fatalf("mysql dsn is required")
	}
	db, err := store.NewDB(store.DBConfig{
		DSN:             dsn,
		MaxOpenConn:     cfg.MySQL.MaxOpenConn,
		MaxIdleConn:     cfg.MySQL.MaxIdleConn,
		ConnMaxLifetime: time.Duration(cfg.MySQL.ConnMaxLifetime) * time.Second,
	})
	if err != nil {
		log.Fatalf("init mysql failed: %v", err)
	}
	defer db.Close()
	log.Info("Config service connected to mysql")

	// Redis
	var redisClient *redis.Client
	var redisAddrList []string
	if len(cfg.Redis.Addrs) > 0 {
		redisAddrList = cfg.Redis.Addrs
	} else if *redisAddrs != "" {
		redisAddrList = strings.Split(*redisAddrs, ",")
	}
	if len(redisAddrList) > 0 {
		redisClient = redis.NewClient(redis.Config{
			Addrs:    redisAddrList,
			Password: cfg.Redis.Password,
			PoolSize: cfg.Redis.PoolSize,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, push dispatcher disabled", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Config service connected to redis")
		}
	}

	// Store
	configStore := store.NewConfigStore(db)

	// Handler
	h := handler.NewConfigHandler(configStore, redisClient, log)

	mux := http.NewServeMux()
	mux.HandleFunc("/api/configs", h.HandleConfigs)               // GET / POST
	mux.HandleFunc("/api/configs/", h.HandleConfigPath)            // /{name} /{name}/publish /{name}/rollback ...
	// Health check
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("ok"))
	})

	// CORS 包装器（允许 Web 管理后台跨域访问）
	handler := withCORS(mux)

	listenAddr := fmt.Sprintf("%s:%d", cfg.Network.Host, cfg.Network.Port)
	server := &http.Server{Addr: listenAddr, Handler: handler}

	// Registry
	var sd discovery.ServiceDiscovery
	if cfg.Discovery.Type != "" {
		sd, err = discovery.New(cfg.Discovery.Type, cfg.Discovery.Addrs)
		if err != nil {
			log.Warnf("init discovery failed: %v, running without registry", err)
		} else {
			node := discovery.NodeInfo{
				ServiceType: cfg.Service.ServiceType,
				NodeID:      cfg.Service.NodeID,
				Host:        cfg.Network.Host,
				Port:        uint32(cfg.Network.Port),
				RegisterAt:  uint64(time.Now().Unix()),
			}
			if err := sd.Register(context.Background(), node); err != nil {
				log.Warnf("register failed: %v", err)
			} else {
				log.Info("Config service registered to registry")
			}
			defer sd.Close()
		}
	}

	go func() {
		log.Infof("Config HTTP server started on %s", listenAddr)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("config server failed: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down config service...")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	server.Shutdown(ctx)
}

func withCORS(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, X-Operator, Authorization")
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusOK)
			return
		}
		next.ServeHTTP(w, r)
	})
}
