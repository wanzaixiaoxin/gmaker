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
	)
	flag.Parse()

	etcdStore, err := store.NewEtcdStore(*etcdAddrs)
	if err != nil {
		log.Fatalf("failed to connect etcd: %v", err)
	}
	defer etcdStore.Close()

	srv := server.New(*listenAddr, etcdStore)
	if err := srv.Start(); err != nil {
		log.Fatalf("failed to start registry server: %v", err)
	}

	log.Printf("Registry server started on %s, etcd=%s", *listenAddr, *etcdAddrs)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down registry server...")
	srv.Stop()
}
