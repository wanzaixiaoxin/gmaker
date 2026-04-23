package net

import (
	"encoding/binary"
	"errors"
	"io"
)

const (
	HeaderSize   = 18
	MaxPacketLen = 16 * 1024 * 1024 // 16MB
	MagicValue   = uint16(0x9D7F)
)

// Flag 位定义
type Flag uint32

const (
	FlagEncrypt   Flag = 1 << 0
	FlagCompress  Flag = 1 << 1
	FlagBroadcast Flag = 1 << 2
	FlagTrace     Flag = 1 << 3
	FlagRPCReq    Flag = 1 << 4
	FlagRPCRes    Flag = 1 << 5
	FlagRPCFF     Flag = 1 << 6
	FlagHeartbeat Flag = 1 << 7
	FlagRoomBcast Flag = 1 << 8 // 按聊天室广播，payload 前 8 字节为 room_id（大端序）
)

// Header 固定 18 字节包头
type Header struct {
	Length uint32 // 整包长度（含自身），大端序
	Magic  uint16 // 0x9D7F
	CmdID  uint32 // 命令号
	SeqID  uint32 // 序列号
	Flags  uint32 // 标志位
}

// Packet 完整消息
type Packet struct {
	Header
	Payload []byte
}

// Encode 将 Packet 编码为字节流，返回新分配的切片
func (p *Packet) Encode() []byte {
	buf := make([]byte, p.Length)
	binary.BigEndian.PutUint32(buf[0:4], p.Length)
	binary.BigEndian.PutUint16(buf[4:6], p.Magic)
	binary.BigEndian.PutUint32(buf[6:10], p.CmdID)
	binary.BigEndian.PutUint32(buf[10:14], p.SeqID)
	binary.BigEndian.PutUint32(buf[14:18], p.Flags)
	copy(buf[18:], p.Payload)
	return buf
}

// DecodeHeader 从 reader 中读取并解析包头
func DecodeHeader(r io.Reader) (*Header, error) {
	buf := make([]byte, 4)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	length := binary.BigEndian.Uint32(buf)
	if length < HeaderSize || length > MaxPacketLen {
		return nil, errors.New("invalid packet length")
	}

	buf = make([]byte, HeaderSize-4)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}

	h := &Header{
		Length: length,
		Magic:  binary.BigEndian.Uint16(buf[0:2]),
		CmdID:  binary.BigEndian.Uint32(buf[2:6]),
		SeqID:  binary.BigEndian.Uint32(buf[6:10]),
		Flags:  binary.BigEndian.Uint32(buf[10:14]),
	}
	if h.Magic != MagicValue {
		return nil, errors.New("invalid magic")
	}
	return h, nil
}

// ReadPayload 从 reader 中读取 payload
func ReadPayload(r io.Reader, h *Header) ([]byte, error) {
	payloadLen := int(h.Length) - HeaderSize
	if payloadLen <= 0 {
		return nil, nil
	}
	payload := make([]byte, payloadLen)
	if _, err := io.ReadFull(r, payload); err != nil {
		return nil, err
	}
	return payload, nil
}
