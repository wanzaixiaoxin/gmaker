// Phase 2 端到端联调测试：登录 -> 读玩家数据 -> 修改 -> 写回
package main

import (
	"log"
	"os"
	"os/exec"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	bizpb "github.com/gmaker/luffa/gen/go/biz"
	loginpb "github.com/gmaker/luffa/gen/go/login"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	registryAddr = "127.0.0.1:2379"
	bizAddr      = "127.0.0.1:8082"
	gatewayAddr  = "127.0.0.1:8081"
	dbproxyAddr  = "127.0.0.1:3307"
)

func main() {
	log.Println("=== Phase 2 Integration Test ===")
	log.Println("Prerequisites: MySQL on 3306, Redis on 6379")

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

	// 2. 启动 DBProxy
	mysqlDSN := "root:123456@tcp(127.0.0.1:3306)/gmaker"
	dbproxyCmd := exec.Command("./bin/dbproxy-go.exe", "-config", "conf/dbproxy.json", "-mysql", mysqlDSN)
	dbproxyCmd.Stdout = os.Stdout
	dbproxyCmd.Stderr = os.Stderr
	if err := dbproxyCmd.Start(); err != nil {
		log.Fatalf("start dbproxy failed: %v", err)
	}
	defer stopCmd(dbproxyCmd)
	time.Sleep(800 * time.Millisecond)
	log.Println("DBProxy started")

	// 3. 启动 Biz
	bizCmd := exec.Command("./bin/biz-go.exe", "-config", "conf/biz.json", "-dbproxy", dbproxyAddr)
	bizCmd.Stdout = os.Stdout
	bizCmd.Stderr = os.Stderr
	if err := bizCmd.Start(); err != nil {
		log.Fatalf("start biz failed: %v", err)
	}
	defer stopCmd(bizCmd)
	time.Sleep(800 * time.Millisecond)
	log.Println("Biz started")

	// 4. 启动 Gateway
	gatewayCmd := exec.Command("./bin/gateway-cpp.exe", "--config", "conf/gateway.json")
	gatewayCmd.Stdout = os.Stdout
	gatewayCmd.Stderr = os.Stderr
	if err := gatewayCmd.Start(); err != nil {
		log.Fatalf("start gateway failed: %v", err)
	}
	defer stopCmd(gatewayCmd)
	time.Sleep(2 * time.Second)
	log.Println("Gateway started")

	// 5. 客户端连接到 Gateway
	var lastPkt *net.Packet
	client := net.NewTCPClient(gatewayAddr,
		func(conn *net.TCPConn, pkt *net.Packet) {
			lastPkt = pkt
			log.Printf("Client received: cmd=0x%08X seq=%d len=%d", pkt.CmdID, pkt.SeqID, len(pkt.Payload))
		},
		func(conn *net.TCPConn) {
			log.Println("Client disconnected from gateway")
		})
	client.SetMasterKey(make([]byte, 32)) // 使用默认零密钥进行握手
	if err := client.Connect(); err != nil {
		log.Fatalf("connect gateway failed: %v", err)
	}
	defer client.Close()

	seq := uint32(1)

	// 6. 登录请求
	loginReq := &loginpb.LoginReq{
		Account:  "test_player",
		Password: "test_password",
		Platform: "pc",
		Version:  "1.0.0",
	}
	lastPkt = nil
	sendPacket(client, uint32(protocol.CmdCommon_CMD_CMN_LOGIN_REQ), seq, loginReq)
	waitForResponse(&lastPkt, 3*time.Second)
	if lastPkt == nil || lastPkt.CmdID != uint32(protocol.CmdCommon_CMD_CMN_LOGIN_RES) {
		log.Fatal("Login response not received")
	}
	var loginRes loginpb.LoginRes
	if err := proto.Unmarshal(lastPkt.Payload, &loginRes); err != nil {
		log.Fatalf("unmarshal login res failed: %v", err)
	}
	if !loginRes.Result.Ok {
		log.Fatalf("login failed: code=%d", loginRes.Result.Code)
	}
	log.Printf("Login success: player_id=%d token=%s", loginRes.PlayerId, loginRes.Token)
	seq++

	playerID := loginRes.PlayerId

	// 7. 获取玩家数据
	getReq := &bizpb.GetPlayerReq{PlayerId: playerID}
	lastPkt = nil
	sendPacket(client, uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ), seq, getReq)
	waitForResponse(&lastPkt, 3*time.Second)
	if lastPkt == nil || lastPkt.CmdID != uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES) {
		log.Fatal("GetPlayer response not received")
	}
	var getRes bizpb.GetPlayerRes
	if err := proto.Unmarshal(lastPkt.Payload, &getRes); err != nil {
		log.Fatalf("unmarshal get player res failed: %v", err)
	}
	if !getRes.Result.Ok {
		log.Fatalf("get player failed: code=%d", getRes.Result.Code)
	}
	log.Printf("GetPlayer success: nickname=%s level=%d coin=%d diamond=%d",
		getRes.Player.Nickname, getRes.Player.Level, getRes.Player.Coin, getRes.Player.Diamond)
	seq++

	// 8. 更新玩家数据
	updateReq := &bizpb.UpdatePlayerReq{
		PlayerId: playerID,
		Nickname: "updated_name",
		Coin:     100,
		Diamond:  50,
	}
	lastPkt = nil
	sendPacket(client, uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_REQ), seq, updateReq)
	waitForResponse(&lastPkt, 3*time.Second)
	if lastPkt == nil || lastPkt.CmdID != uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_RES) {
		log.Fatal("UpdatePlayer response not received")
	}
	var updateRes bizpb.UpdatePlayerRes
	if err := proto.Unmarshal(lastPkt.Payload, &updateRes); err != nil {
		log.Fatalf("unmarshal update player res failed: %v", err)
	}
	if !updateRes.Result.Ok {
		log.Fatalf("update player failed: code=%d", updateRes.Result.Code)
	}
	log.Println("UpdatePlayer success")
	seq++

	// 9. 再次获取玩家数据，验证修改已持久化
	getReq2 := &bizpb.GetPlayerReq{PlayerId: playerID}
	lastPkt = nil
	sendPacket(client, uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ), seq, getReq2)
	waitForResponse(&lastPkt, 3*time.Second)
	if lastPkt == nil || lastPkt.CmdID != uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES) {
		log.Fatal("GetPlayer verification response not received")
	}
	var getRes2 bizpb.GetPlayerRes
	if err := proto.Unmarshal(lastPkt.Payload, &getRes2); err != nil {
		log.Fatalf("unmarshal get player res2 failed: %v", err)
	}
	if !getRes2.Result.Ok {
		log.Fatalf("get player verification failed: code=%d", getRes2.Result.Code)
	}
	if getRes2.Player.Nickname != "updated_name" || getRes2.Player.Coin != 100 || getRes2.Player.Diamond != 50 {
		log.Fatalf("player data mismatch after update: got nickname=%s coin=%d diamond=%d",
			getRes2.Player.Nickname, getRes2.Player.Coin, getRes2.Player.Diamond)
	}
	log.Printf("GetPlayer verification success: nickname=%s coin=%d diamond=%d",
		getRes2.Player.Nickname, getRes2.Player.Coin, getRes2.Player.Diamond)

	log.Println("=== Phase 2 Integration Test PASSED ===")

	// 自动退出（CI 友好）
	time.Sleep(1 * time.Second)
}

func sendPacket(client *net.TCPClient, cmdID uint32, seqID uint32, msg proto.Message) {
	payload, err := proto.Marshal(msg)
	if err != nil {
		log.Fatalf("marshal failed: %v", err)
	}
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seqID,
			Flags:  uint32(net.FlagRPCReq),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}
	if !client.Conn().SendPacket(pkt) {
		log.Fatal("send packet failed")
	}
}

func waitForResponse(pkt **net.Packet, timeout time.Duration) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if *pkt != nil {
			return
		}
		time.Sleep(50 * time.Millisecond)
	}
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
