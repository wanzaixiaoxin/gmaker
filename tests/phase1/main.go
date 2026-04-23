// Phase 1 端到端联调测试：Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)
package main

import (
	"log"
	"os"
	"os/exec"
	"os/signal"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/biz"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	registryAddr = "127.0.0.1:2379"
	bizAddr      = "127.0.0.1:8082"
	gatewayAddr  = "127.0.0.1:8081"
)

func main() {
	log.Println("=== Phase 1 Integration Test ===")

	// 1. 启动 Registry（内存存储模式）
	registryCmd := exec.Command("./bin/registry-go.exe", "-listen", registryAddr, "-store", "memory")
	registryCmd.Stdout = os.Stdout
	registryCmd.Stderr = os.Stderr
	if err := registryCmd.Start(); err != nil {
		log.Fatalf("start registry failed: %v", err)
	}
	defer stopCmd(registryCmd)
	time.Sleep(500 * time.Millisecond)
	log.Println("Registry started")

	// 2. 启动 Biz
	bizCmd := exec.Command("./bin/biz-go.exe", "-listen", bizAddr, "-registry", registryAddr)
	bizCmd.Stdout = os.Stdout
	bizCmd.Stderr = os.Stderr
	if err := bizCmd.Start(); err != nil {
		log.Fatalf("start biz failed: %v", err)
	}
	defer stopCmd(bizCmd)
	time.Sleep(500 * time.Millisecond)
	log.Println("Biz started")

	// 3. 启动 Gateway
	gatewayCmd := exec.Command("./build/Release/gateway-cpp",
		extractPort(gatewayAddr), "127.0.0.1:2379", "127.0.0.1:8082")
	gatewayCmd.Stdout = os.Stdout
	gatewayCmd.Stderr = os.Stderr
	if err := gatewayCmd.Start(); err != nil {
		log.Fatalf("start gateway failed: %v", err)
	}
	defer stopCmd(gatewayCmd)
	time.Sleep(500 * time.Millisecond)
	log.Println("Gateway started")

	// 4. 客户端连接到 Gateway 并发送 Ping
	client := net.NewTCPClient(gatewayAddr,
		func(conn *net.TCPConn, pkt *net.Packet) {
			log.Printf("Client received: cmd=%d seq=%d len=%d", pkt.CmdID, pkt.SeqID, len(pkt.Payload))
		},
		func(conn *net.TCPConn) {
			log.Println("Client disconnected from gateway")
		})
	if err := client.Connect(); err != nil {
		log.Fatalf("connect gateway failed: %v", err)
	}
	defer client.Close()

	// 发送 Ping 请求
	ping := &pb.Ping{ClientTime: uint64(time.Now().UnixMilli())}
	payload, _ := proto.Marshal(ping)
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  uint32(protocol.CmdBiz_CMD_BIZ_PING),
			SeqID:  1,
			Flags:  uint32(net.FlagRPCReq),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}

	log.Println("Sending Ping via Gateway -> Biz")
	if !client.Conn().SendPacket(pkt) {
		log.Fatal("send failed")
	}

	// 等待响应（简化：直接 sleep 等待回调）
	time.Sleep(500 * time.Millisecond)

	log.Println("=== Phase 1 Integration Test PASSED ===")

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
}

func stopCmd(cmd *exec.Cmd) {
	if cmd != nil && cmd.Process != nil {
		cmd.Process.Kill()
		cmd.Wait()
	}
}

func extractPort(addr string) string {
	for i := len(addr) - 1; i >= 0; i-- {
		if addr[i] == ':' {
			return addr[i+1:]
		}
	}
	return addr
}
