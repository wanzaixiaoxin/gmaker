package net

// PacketSender 抽象发送接口，支持单连接和连接池
type PacketSender interface {
	SendPacket(pkt *Packet) bool
}
