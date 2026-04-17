#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>

namespace gs {
namespace net {

constexpr uint32_t HEADER_SIZE = 18;
constexpr uint32_t MAX_PACKET_LEN = 16 * 1024 * 1024; // 16MB
constexpr uint16_t MAGIC_VALUE = 0x9D7F;

// Flag 位定义
enum class Flag : uint32_t {
    ENCRYPT   = 1u << 0,
    COMPRESS  = 1u << 1,
    BROADCAST = 1u << 2,
    TRACE     = 1u << 3,
    RPC_REQ   = 1u << 4,
    RPC_RES   = 1u << 5,
    RPC_FF    = 1u << 6,
    HEARTBEAT = 1u << 7,
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
    std::vector<uint8_t> payload;
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

// 编码 Packet 为字节流
inline std::vector<uint8_t> EncodePacket(const Packet& pkt) {
    std::vector<uint8_t> buf(pkt.header.length);
    WriteU32BE(buf.data() + 0, pkt.header.length);
    WriteU16BE(buf.data() + 4, pkt.header.magic);
    WriteU32BE(buf.data() + 6, pkt.header.cmd_id);
    WriteU32BE(buf.data() + 10, pkt.header.seq_id);
    WriteU32BE(buf.data() + 14, pkt.header.flags);
    if (!pkt.payload.empty()) {
        std::memcpy(buf.data() + HEADER_SIZE, pkt.payload.data(), pkt.payload.size());
    }
    return buf;
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
