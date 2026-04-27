#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "gs/net/buffer.hpp"

namespace gs {
namespace gateway {
namespace websocket {

struct WSFrame {
    bool fin = true;
    uint8_t opcode = 0;
    bool masked = false;
    uint64_t payload_len = 0;
    uint8_t mask_key[4] = {0};
};

// 尝试从 data 中解析一个完整的 WebSocket 帧
// 返回值: >0 表示成功，为消耗的总字节数（header + payload）
//         0 表示数据不足
//        -1 表示协议错误
intptr_t ParseWSFrame(const uint8_t* data, size_t len, WSFrame& out, size_t& payload_offset);

// 对 payload 进行 in-place unmask
void UnmaskWSPayload(uint8_t* payload, size_t len, const uint8_t mask_key[4]);

// 服务器 -> 客户端 的帧编码（无 mask）
gs::net::Buffer EncodeWSBinaryFrame(const uint8_t* payload, size_t len);
gs::net::Buffer EncodeWSCloseFrame(uint16_t code = 1000);
gs::net::Buffer EncodeWSPongFrame();

// 计算 Sec-WebSocket-Accept（SHA1 + Base64）
std::string ComputeWSAccept(const std::string& ws_key);

} // namespace websocket
} // namespace gateway
} // namespace gs
