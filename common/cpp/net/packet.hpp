#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "buffer.hpp"

namespace gs {
namespace net {

constexpr uint32_t HEADER_SIZE = 18;
constexpr uint32_t MAX_PACKET_LEN = 16 * 1024 * 1024; // 16MB
constexpr uint16_t MAGIC_VALUE = 0x9D7F;

// Flag 位定义
enum class Flag : uint32_t {
    ENCRYPT     = 1u << 0,
    COMPRESS    = 1u << 1,
    BROADCAST   = 1u << 2,
    TRACE       = 1u << 3,
    RPC_REQ     = 1u << 4,
    RPC_RES     = 1u << 5,
    RPC_FF      = 1u << 6,
    HEARTBEAT   = 1u << 7,
    ROOM_BCAST  = 1u << 8,  // 按聊天室广播，payload 前 8 字节为 room_id（大端序）
};

inline uint32_t operator|(Flag a, Flag b) {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}
inline uint32_t operator|(uint32_t a, Flag b) {
    return a | static_cast<uint32_t>(b);
}
inline bool HasFlag(uint32_t flags, Flag f) {
    return (flags & static_cast<uint32_t>(f)) != 0;
}

// 包头结构（大端序）
struct Header {
    uint32_t length = 0; // 整包长度
    uint16_t magic  = MAGIC_VALUE;
    uint32_t cmd_id = 0;
    uint32_t seq_id = 0;
    uint32_t flags  = 0;
};

struct Packet {
    Header header;
    Buffer payload;  // 引用计数缓冲区，支持零拷贝广播
};

// 工具函数：主机字节序 -> 大端序
inline void WriteU32BE(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8)  & 0xFF);
    p[3] = static_cast<uint8_t>( v        & 0xFF);
}
inline void WriteU16BE(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>( v       & 0xFF);
}
inline uint32_t ReadU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}
inline uint16_t ReadU16BE(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) |
            static_cast<uint16_t>(p[1]);
}
inline void WriteU64BE(uint8_t* p, uint64_t v) {
    p[0] = static_cast<uint8_t>((v >> 56) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 48) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 40) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 32) & 0xFF);
    p[4] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[5] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[6] = static_cast<uint8_t>((v >> 8)  & 0xFF);
    p[7] = static_cast<uint8_t>( v        & 0xFF);
}
inline uint64_t ReadU64BE(const uint8_t* p) {
    return (static_cast<uint64_t>(p[0]) << 56) |
           (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) |
           (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) |
           (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) << 8)  |
            static_cast<uint64_t>(p[7]);
}

// 编码 Packet 为字节流（返回 Buffer，支持零拷贝共享）
inline Buffer EncodePacket(const Packet& pkt) {
    size_t payload_size = pkt.payload.Size();
    std::vector<uint8_t> buf(HEADER_SIZE + payload_size);
    uint32_t total_len = HEADER_SIZE + static_cast<uint32_t>(payload_size);
    WriteU32BE(buf.data() + 0, total_len);
    WriteU16BE(buf.data() + 4, pkt.header.magic);
    WriteU32BE(buf.data() + 6, pkt.header.cmd_id);
    WriteU32BE(buf.data() + 10, pkt.header.seq_id);
    WriteU32BE(buf.data() + 14, pkt.header.flags);
    if (payload_size > 0 && pkt.payload.Data()) {
        std::memcpy(buf.data() + HEADER_SIZE, pkt.payload.Data(), payload_size);
    }
    return Buffer::FromVector(std::move(buf));
}

// 从缓冲区解析包头（假设缓冲区至少有 HEADER_SIZE 字节）
inline Header DecodeHeader(const uint8_t* data) {
    Header h;
    h.length = ReadU32BE(data + 0);
    h.magic  = ReadU16BE(data + 4);
    h.cmd_id = ReadU32BE(data + 6);
    h.seq_id = ReadU32BE(data + 10);
    h.flags  = ReadU32BE(data + 14);
    return h;
}

} // namespace net
} // namespace gs
