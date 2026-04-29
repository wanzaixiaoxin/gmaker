#pragma once

#include "net/buffer.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace gs {
namespace net {
namespace websocket {

enum class MessageType : uint8_t {
    Text = 0x1,
    Binary = 0x2
};

struct WSFrame {
    bool fin = true;
    uint8_t opcode = 0;
    bool masked = false;
    uint64_t payload_len = 0;
    uint8_t mask_key[4] = {0};
};

constexpr uint8_t OPCODE_CONTINUATION = 0x0;
constexpr uint8_t OPCODE_TEXT = 0x1;
constexpr uint8_t OPCODE_BINARY = 0x2;
constexpr uint8_t OPCODE_CLOSE = 0x8;
constexpr uint8_t OPCODE_PING = 0x9;
constexpr uint8_t OPCODE_PONG = 0xA;

// Returns consumed bytes on success, 0 when more data is needed, and -1 on protocol error.
intptr_t ParseWSFrame(const uint8_t* data, size_t len, WSFrame& out, size_t& payload_offset);

void UnmaskWSPayload(uint8_t* payload, size_t len, const uint8_t mask_key[4]);

// Server-to-client frames are not masked.
gs::net::Buffer EncodeWSFrameHeader(uint8_t opcode, size_t payload_len);
gs::net::Buffer EncodeWSFrame(uint8_t opcode, const uint8_t* payload, size_t len);
gs::net::Buffer EncodeWSBinaryFrame(const uint8_t* payload, size_t len);
gs::net::Buffer EncodeWSTextFrame(const uint8_t* payload, size_t len);
gs::net::Buffer EncodeWSCloseFrame(uint16_t code = 1000, const uint8_t* reason = nullptr, size_t reason_len = 0);
gs::net::Buffer EncodeWSPongFrame(const uint8_t* payload = nullptr, size_t len = 0);

std::string ComputeWSAccept(const std::string& ws_key);

} // namespace websocket
} // namespace net
} // namespace gs
