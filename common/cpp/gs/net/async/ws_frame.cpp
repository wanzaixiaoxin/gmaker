#include "ws_frame.hpp"
#include <cstring>

namespace gs {
namespace gateway {
namespace websocket {

// ==================== SHA1 (标准实现，无外部依赖) ====================

namespace {

struct SHA1Ctx {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
};

inline uint32_t rol(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32 - bits));
}

void SHA1Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t W[80];
    // 大端序解析 16 个 32 位字
    for (int i = 0; i < 16; ++i) {
        W[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    // 扩展为 80 个字
    for (int i = 16; i < 80; ++i) {
        W[i] = rol(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 20; ++i) {
        uint32_t f = (b & c) | ((~b) & d);
        uint32_t temp = rol(a, 5) + f + e + W[i] + 0x5A827999;
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
    }
    for (int i = 20; i < 40; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t temp = rol(a, 5) + f + e + W[i] + 0x6ED9EBA1;
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
    }
    for (int i = 40; i < 60; ++i) {
        uint32_t f = (b & c) | (b & d) | (c & d);
        uint32_t temp = rol(a, 5) + f + e + W[i] + 0x8F1BBCDC;
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
    }
    for (int i = 60; i < 80; ++i) {
        uint32_t f = b ^ c ^ d;
        uint32_t temp = rol(a, 5) + f + e + W[i] + 0xCA62C1D6;
        e = d; d = c; c = rol(b, 30); b = a; a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void SHA1Init(SHA1Ctx* ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
}

void SHA1Update(SHA1Ctx* ctx, const uint8_t* data, size_t len) {
    size_t i, j;
    j = (size_t)(ctx->count >> 3) & 63;
    ctx->count += (uint64_t)len << 3;
    if ((j + len) > 63) {
        memcpy(&ctx->buffer[j], data, (i = 64 - j));
        SHA1Transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64) {
            SHA1Transform(ctx->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

void SHA1Final(uint8_t digest[20], SHA1Ctx* ctx) {
    uint64_t finalcount = ctx->count;
    size_t i = (size_t)((finalcount >> 3) & 63);
    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        memset(ctx->buffer + i, 0, 64 - i);
        SHA1Transform(ctx->state, ctx->buffer);
        i = 0;
    }
    memset(ctx->buffer + i, 0, 56 - i);
    for (int b = 0; b < 8; ++b) {
        ctx->buffer[56 + b] = (uint8_t)((finalcount >> ((7 - b) * 8)) & 0xFF);
    }
    SHA1Transform(ctx->state, ctx->buffer);
    for (i = 0; i < 20; ++i) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
    }
}

std::vector<uint8_t> SHA1Raw(const uint8_t* data, size_t len) {
    SHA1Ctx ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, data, len);
    std::vector<uint8_t> digest(20);
    SHA1Final(digest.data(), &ctx);
    return digest;
}

// ==================== Base64 (极简实现) ====================

const char BASE64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = data[i] << 16;
        if (i + 1 < len) v |= data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        out.push_back(BASE64_CHARS[(v >> 18) & 0x3F]);
        out.push_back(BASE64_CHARS[(v >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? BASE64_CHARS[(v >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? BASE64_CHARS[v & 0x3F] : '=');
    }
    return out;
}

} // anonymous namespace

// ==================== WebSocket 帧解析 / 编码 ====================

intptr_t ParseWSFrame(const uint8_t* data, size_t len, WSFrame& out, size_t& payload_offset) {
    if (len < 2) return 0;

    out.fin = (data[0] & 0x80) != 0;
    out.opcode = data[0] & 0x0F;
    out.masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    size_t pos = 2;
    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        pos = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        pos = 10;
    }

    if (out.masked) {
        if (len < pos + 4) return 0;
        memcpy(out.mask_key, data + pos, 4);
        pos += 4;
    }

    if (len < pos + payload_len) return 0;

    out.payload_len = payload_len;
    payload_offset = pos;
    return static_cast<intptr_t>(pos + payload_len);
}

void UnmaskWSPayload(uint8_t* payload, size_t len, const uint8_t mask_key[4]) {
    for (size_t i = 0; i < len; ++i) {
        payload[i] ^= mask_key[i & 3];
    }
}

gs::net::Buffer EncodeWSBinaryFrame(const uint8_t* payload, size_t len) {
    std::vector<uint8_t> frame;
    frame.push_back(0x82); // FIN=1, opcode=binary
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }
    frame.insert(frame.end(), payload, payload + len);
    return gs::net::Buffer::FromVector(std::move(frame));
}

gs::net::Buffer EncodeWSCloseFrame(uint16_t code) {
    std::vector<uint8_t> frame;
    frame.push_back(0x88); // FIN=1, opcode=close
    frame.push_back(2);
    frame.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(code & 0xFF));
    return gs::net::Buffer::FromVector(std::move(frame));
}

gs::net::Buffer EncodeWSPongFrame() {
    std::vector<uint8_t> frame;
    frame.push_back(0x8A); // FIN=1, opcode=pong
    frame.push_back(0);
    return gs::net::Buffer::FromVector(std::move(frame));
}

std::string ComputeWSAccept(const std::string& ws_key) {
    static const std::string kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string input = ws_key + kMagic;
    auto digest = SHA1Raw(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    return Base64Encode(digest.data(), digest.size());
}

} // namespace websocket
} // namespace gateway
} // namespace gs
