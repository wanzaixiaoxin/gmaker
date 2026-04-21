package main

import (
	"flag"
	"os"
	"os/signal"
	"syscall"

	"github.com/gmaker/game-server/common/go/logger"
	"github.com/gmaker/game-server/services/registry-go/internal/server"
	"github.com/gmaker/game-server/services/registry-go/internal/store"
)

func main() {
	var (
		listenAddr = flag.String("listen", ":2379", "Registry listen address")
		etcdAddrs  = flag.String("etcd", "127.0.0.1:2379", "Etcd endpoints, comma separated")
		storeType  = flag.String("store", "memory", "Store backend: memory | etcd")
		logFile    = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel   = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	log := logger.NewWithService("registry", "registry-1")
	log.SetLevel(*logLevel)
	if *logFile != "" {
		f, err := os.OpenFile(*logFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
		if err == nil {
			logger.SetDefault(log)
			log = logger.New(logger.Config{Level: *logLevel, Service: "registry", NodeID: "registry-1", Output: f})
		}
	}

	var s store.Store
	var err error

	switch *storeType {
	case "etcd":
		s, err = store.NewEtcdStore(*etcdAddrs)
		if err != nil {
			log.Fatalf("failed to connect etcd: %v", err)
		}
		log.Infof("Registry server started on %s, store=etcd, etcd=%s", *listenAddr, *etcdAddrs)
	default:
		s = store.NewMemoryStore()
		log.Infof("Registry server started on %s, store=memory", *listenAddr)
	}
	defer s.Close()

	srv := server.New(*listenAddr, s)
	if err := srv.Start(); err != nil {
		log.Fatalf("failed to start registry server: %v", err)
	}

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down registry server...")
	srv.Stop()
}
