package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/gmaker/game-server/common/go/net"
	"github.com/gmaker/game-server/common/go/registry"
	pb "github.com/gmaker/game-server/gen/go/registry"
)

func main() {
	var (
		listenAddr = flag.String("listen", ":8082", "Biz listen address")
		registryAddr = flag.String("registry", "127.0.0.1:2379", "Registry address")
	)
	flag.Parse()

	// 注册到 Registry
	regClient := registry.NewClient(*registryAddr)
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "biz",
		NodeId:      "biz-001",
		Host:        "127.0.0.1",
		Port:        8082,
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.Register(node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Println("Biz registered to registry")

	// 启动 TCP 服务
	cfg := net.ServerConfig{
		Addr: *listenAddr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			log.Printf("Biz received cmd=%d seq=%d len=%d", pkt.CmdID, pkt.SeqID, len(pkt.Payload))
			// Echo back a pong
			reply := &net.Packet{
				Header: net.Header{
					Magic:  net.MagicValue,
					CmdID:  pkt.CmdID,
					SeqID:  pkt.SeqID,
					Flags:  uint32(net.FlagRPCRes),
					Length: net.HeaderSize,
				},
			}
			conn.SendPacket(reply)
		},
		OnClose: func(conn *net.TCPConn) {
			log.Printf("Biz connection closed: %s", conn.ID())
		},
	}
	srv := net.NewTCPServer(cfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start biz server failed: %v", err)
	}
	log.Printf("Biz server started on %s", *listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down biz server...")
	srv.Stop()
}
