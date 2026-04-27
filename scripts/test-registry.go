package main

import (
	"context"
	"fmt"
	"time"

	"github.com/gmaker/luffa/common/go/discovery"
)

func main() {
	sd, err := discovery.New("registry", []string{"127.0.0.1:2379"})
	if err != nil {
		fmt.Printf("discovery init failed: %v\n", err)
		return
	}
	defer sd.Close()
	// Trigger connection by registering a dummy node
	_ = sd.Register(context.Background(), discovery.NodeInfo{
		ServiceType: "test",
		NodeID:      "test-1",
		Host:        "127.0.0.1",
		Port:        1,
		RegisterAt:  uint64(time.Now().Unix()),
	})

	for _, svc := range []string{"gateway", "biz", "chat", "login", "dbproxy"} {
		nodes, err := sd.Discover(context.Background(), svc)
		if err != nil {
			fmt.Printf("Discover %s failed: %v\n", svc, err)
			continue
		}
		fmt.Printf("%s: %d nodes\n", svc, len(nodes))
		for _, n := range nodes {
			fmt.Printf("  - %s @ %s:%d\n", n.NodeID, n.Host, n.Port)
		}
	}
}
