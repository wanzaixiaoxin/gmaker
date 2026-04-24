package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/registry"
	pb "github.com/gmaker/luffa/gen/go/registry"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/mysql"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/server"
)

func main() {
	var (
		listenAddr    = flag.String("listen", ":3307", "DBProxy listen address")
		mysqlDSNs     = flag.String("mysql", "", "MySQL DSNs, comma separated")
		registryAddrs = flag.String("registry", "127.0.0.1:2379", "Registry addresses, comma separated")
		logFile       = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel      = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	log := logger.InitServiceLogger("dbproxy", "dbproxy-1", *logLevel, *logFile)
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

	// 注册到 Registry
	regClient := registry.NewClient(strings.Split(*registryAddrs, ","))
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "dbproxy",
		NodeId:      "dbproxy-1",
		Host:        "127.0.0.1",
		Port:        parsePort(*listenAddr),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.RegisterWithRetry(node, 5); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("DBProxy registered to registry")

	// 启动心跳（每 5 秒）
	heartbeatStop := make(chan struct{})
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				if _, err := regClient.Heartbeat(node.NodeId); err != nil {
					log.Warnf("heartbeat failed: %v", err)
				}
			case <-heartbeatStop:
				return
			}
		}
	}()
	defer close(heartbeatStop)

	srv := server.New(*listenAddr, mproxy)
	if err := srv.Start(); err != nil {
		log.Fatalf("start dbproxy failed: %v", err)
	}
	log.Infof("DBProxy started on %s", *listenAddr)

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
