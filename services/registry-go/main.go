package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"syscall"

	"registry-go/internal/server"
	"registry-go/internal/store"
)

func main() {
	var (
		listenAddr = flag.String("listen", ":2379", "Registry listen address")
		etcdAddrs  = flag.String("etcd", "127.0.0.1:2379", "Etcd endpoints, comma separated")
		storeType  = flag.String("store", "memory", "Store backend: memory | etcd")
	)
	flag.Parse()

	var s store.Store
	var err error

	switch *storeType {
	case "etcd":
		s, err = store.NewEtcdStore(*etcdAddrs)
		if err != nil {
			log.Fatalf("failed to connect etcd: %v", err)
		}
		log.Printf("Registry server started on %s, store=etcd, etcd=%s", *listenAddr, *etcdAddrs)
	default:
		s = store.NewMemoryStore()
		log.Printf("Registry server started on %s, store=memory", *listenAddr)
	}
	defer s.Close()

	srv := server.New(*listenAddr, s)
	if err := srv.Start(); err != nil {
		log.Fatalf("failed to start registry server: %v", err)
	}

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down registry server...")
	srv.Stop()
}
