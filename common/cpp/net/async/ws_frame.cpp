#include "ws_frame.hpp"
#include <cstring>
#include <vector>

namespace gs {
namespace net {
namespace websocket {

namespace {

struct SHA1Ctx {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
};

inline uint32_t rol(uint32_t value, unsigned int bits) {
    return (value << bits) | (value >> (32 - bits));
}

inline uint32_t blk(uint32_t b[16], unsigned int i) {
    return b[i & 15] = rol(b[(i + 13) & 15] ^ b[(i + 8) & 15] ^ b[(i + 2) & 15] ^ b[i & 15], 1);
}

#define R0(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + b[i] + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R1(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + blk(b, i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define R2(v,w,x,y,z,i) z += (w ^ x ^ y) + blk(b, i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30);
#define R3(v,w,x,y,z,i) z += (((w | x) & y) | (w & x)) + blk(b, i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30);
#define R4(v,w,x,y,z,i) z += (w ^ x ^ y) + blk(b, i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30);

void SHA1Transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a = state[0], b[16];
    for (int i = 0; i < 16; ++i) {
        b[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               ((uint32_t)buffer[i * 4 + 3]);
    }
    uint32_t b_state[4] = { state[1], state[2], state[3], state[4] };
    uint32_t &b0 = b_state[0], &b1 = b_state[1], &b2 = b_state[2], &b3 = b_state[3];

    R0(a, b0, b1, b2, b3, 0); R0(b3, a, b0, b1, b2, 1); R0(b2, b3, a, b0, b1, 2); R0(b1, b2, b3, a, b0, 3);
    R0(b0, b1, b2, b3, a, 4); R0(a, b0, b1, b2, b3, 5); R0(b3, a, b0, b1, b2, 6); R0(b2, b3, a, b0, b1, 7);
    R0(b1, b2, b3, a, b0, 8); R0(b0, b1, b2, b3, a, 9); R0(a, b0, b1, b2, b3, 10); R0(b3, a, b0, b1, b2, 11);
    R0(b2, b3, a, b0, b1, 12); R0(b1, b2, b3, a, b0, 13); R0(b0, b1, b2, b3, a, 14); R0(a, b0, b1, b2, b3, 15);
    R1(b3, a, b0, b1, b2, 16); R1(b2, b3, a, b0, b1, 17); R1(b1, b2, b3, a, b0, 18); R1(b0, b1, b2, b3, a, 19);
    R2(a, b0, b1, b2, b3, 20); R2(b3, a, b0, b1, b2, 21); R2(b2, b3, a, b0, b1, 22); R2(b1, b2, b3, a, b0, 23);
    R2(b0, b1, b2, b3, a, 24); R2(a, b0, b1, b2, b3, 25); R2(b3, a, b0, b1, b2, 26); R2(b2, b3, a, b0, b1, 27);
    R2(b1, b2, b3, a, b0, 28); R2(b0, b1, b2, b3, a, 29); R2(a, b0, b1, b2, b3, 30); R2(b3, a, b0, b1, b2, 31);
    R2(b2, b3, a, b0, b1, 32); R2(b1, b2, b3, a, b0, 33); R2(b0, b1, b2, b3, a, 34); R2(a, b0, b1, b2, b3, 35);
    R2(b3, a, b0, b1, b2, 36); R2(b2, b3, a, b0, b1, 37); R2(b1, b2, b3, a, b0, 38); R2(b0, b1, b2, b3, a, 39);
    R3(a, b0, b1, b2, b3, 40); R3(b3, a, b0, b1, b2, 41); R3(b2, b3, a, b0, b1, 42); R3(b1, b2, b3, a, b0, 43);
    R3(b0, b1, b2, b3, a, 44); R3(a, b0, b1, b2, b3, 45); R3(b3, a, b0, b1, b2, 46); R3(b2, b3, a, b0, b1, 47);
    R3(b1, b2, b3, a, b0, 48); R3(b0, b1, b2, b3, a, 49); R3(a, b0, b1, b2, b3, 50); R3(b3, a, b0, b1, b2, 51);
    R3(b2, b3, a, b0, b1, 52); R3(b1, b2, b3, a, b0, 53); R3(b0, b1, b2, b3, a, 54); R3(a, b0, b1, b2, b3, 55);
    R3(b3, a, b0, b1, b2, 56); R3(b2, b3, a, b0, b1, 57); R3(b1, b2, b3, a, b0, 58); R3(b0, b1, b2, b3, a, 59);
    R4(a, b0, b1, b2, b3, 60); R4(b3, a, b0, b1, b2, 61); R4(b2, b3, a, b0, b1, 62); R4(b1, b2, b3, a, b0, 63);
    R4(b0, b1, b2, b3, a, 64); R4(a, b0, b1, b2, b3, 65); R4(b3, a, b0, b1, b2, 66); R4(b2, b3, a, b0, b1, 67);
    R4(b1, b2, b3, a, b0, 68); R4(b0, b1, b2, b3, a, 69); R4(a, b0, b1, b2, b3, 70); R4(b3, a, b0, b1, b2, 71);
    R4(b2, b3, a, b0, b1, 72); R4(b1, b2, b3, a, b0, 73); R4(b0, b1, b2, b3, a, 74); R4(a, b0, b1, b2, b3, 75);
    R4(b3, a, b0, b1, b2, 76); R4(b2, b3, a, b0, b1, 77); R4(b1, b2, b3, a, b0, 78); R4(b0, b1, b2, b3, a, 79);

    state[0] += a;
    state[1] += b_state[0];
    state[2] += b_state[1];
    state[3] += b_state[2];
    state[4] += b_state[3];
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

bool IsKnownOpcode(uint8_t opcode) {
    return opcode == OPCODE_CONTINUATION ||
           opcode == OPCODE_TEXT ||
           opcode == OPCODE_BINARY ||
           opcode == OPCODE_CLOSE ||
           opcode == OPCODE_PING ||
           opcode == OPCODE_PONG;
}

bool IsControlOpcode(uint8_t opcode) {
    return (opcode & 0x8) != 0;
}

} // namespace

intptr_t ParseWSFrame(const uint8_t* data, size_t len, WSFrame& out, size_t& payload_offset) {
    if (len < 2) return 0;

    if ((data[0] & 0x70) != 0) return -1;

    out.fin = (data[0] & 0x80) != 0;
    out.opcode = data[0] & 0x0F;
    out.masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    if (!IsKnownOpcode(out.opcode)) return -1;
    if (IsControlOpcode(out.opcode)) {
        if (!out.fin || payload_len > 125) return -1;
    }

    size_t pos = 2;
    if (payload_len == 126) {
        if (len < 4) return 0;
        payload_len = ((uint64_t)data[2] << 8) | data[3];
        if (payload_len < 126) return -1;
        pos = 4;
    } else if (payload_len == 127) {
        if (len < 10) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | data[2 + i];
        }
        if ((data[2] & 0x80) != 0 || payload_len < 65536) return -1;
        pos = 10;
    }

    if (IsControlOpcode(out.opcode) && payload_len > 125) return -1;

    if (out.masked) {
        if (len < pos + 4) return 0;
        memcpy(out.mask_key, data + pos, 4);
        pos += 4;
    }

    if (payload_len > static_cast<uint64_t>(SIZE_MAX - pos)) return -1;
    if (len < pos + static_cast<size_t>(payload_len)) return 0;

    out.payload_len = payload_len;
    payload_offset = pos;
    return static_cast<intptr_t>(pos + payload_len);
}

void UnmaskWSPayload(uint8_t* payload, size_t len, const uint8_t mask_key[4]) {
    for (size_t i = 0; i < len; ++i) {
        payload[i] ^= mask_key[i & 3];
    }
}

gs::net::Buffer EncodeWSFrameHeader(uint8_t opcode, size_t payload_len) {
    std::vector<uint8_t> frame;
    frame.reserve(10);
    frame.push_back(static_cast<uint8_t>(0x80 | (opcode & 0x0F)));
    if (payload_len < 126) {
        frame.push_back(static_cast<uint8_t>(payload_len));
    } else if (payload_len <= 0xFFFF) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload_len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_len & 0xFF));
    } else {
        frame.push_back(127);
        uint64_t len64 = static_cast<uint64_t>(payload_len);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len64 >> (i * 8)) & 0xFF));
        }
    }
    return gs::net::Buffer::FromVector(std::move(frame));
}

gs::net::Buffer EncodeWSFrame(uint8_t opcode, const uint8_t* payload, size_t len) {
    std::vector<uint8_t> frame = EncodeWSFrameHeader(opcode, len).ToVector();
    if (len > 0 && payload) {
        frame.insert(frame.end(), payload, payload + len);
    }
    return gs::net::Buffer::FromVector(std::move(frame));
}

gs::net::Buffer EncodeWSBinaryFrame(const uint8_t* payload, size_t len) {
    return EncodeWSFrame(OPCODE_BINARY, payload, len);
}

gs::net::Buffer EncodeWSTextFrame(const uint8_t* payload, size_t len) {
    return EncodeWSFrame(OPCODE_TEXT, payload, len);
}

gs::net::Buffer EncodeWSCloseFrame(uint16_t code, const uint8_t* reason, size_t reason_len) {
    if (reason_len > 123) reason_len = 123;
    std::vector<uint8_t> payload(2 + reason_len);
    payload[0] = static_cast<uint8_t>((code >> 8) & 0xFF);
    payload[1] = static_cast<uint8_t>(code & 0xFF);
    if (reason_len > 0 && reason) {
        std::memcpy(payload.data() + 2, reason, reason_len);
    }
    return EncodeWSFrame(OPCODE_CLOSE, payload.data(), payload.size());
}

gs::net::Buffer EncodeWSPongFrame(const uint8_t* payload, size_t len) {
    if (len > 125) len = 125;
    return EncodeWSFrame(OPCODE_PONG, payload, len);
}

std::string ComputeWSAccept(const std::string& ws_key) {
    static const std::string kMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string input = ws_key + kMagic;
    auto digest = SHA1Raw(reinterpret_cast<const uint8_t*>(input.data()), input.size());
    return Base64Encode(digest.data(), digest.size());
}

} // namespace websocket
} // namespace net
} // namespace gs
