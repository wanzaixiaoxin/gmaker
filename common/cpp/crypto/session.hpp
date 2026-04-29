#pragma once

#include <vector>
#include <stdexcept>

namespace gs {
namespace crypto {

// 使用 HMAC-SHA256 派生 session key（返回 32 字节）
std::vector<uint8_t> DeriveSessionKey(const std::vector<uint8_t>& masterKey,
                                       const std::vector<uint8_t>& clientRandom);
std::vector<uint8_t> DeriveSessionKey(const std::vector<uint8_t>& masterKey,
                                       const std::vector<uint8_t>& clientRandom,
                                       const std::vector<uint8_t>& serverRandom);

// 生成 n 字节随机数据
std::vector<uint8_t> RandomBytes(size_t n);

// AES-256-GCM 加密 packet payload
std::vector<uint8_t> EncryptPacketPayload(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& payload);

// AES-256-GCM 解密 packet payload
std::vector<uint8_t> DecryptPacketPayload(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& encryptedPayload);



} // namespace crypto
} // namespace gs
