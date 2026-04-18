#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace gs {
namespace crypto {

// AES-256-GCM 加密，返回 nonce || ciphertext || tag（与 Go 侧保持一致）
std::vector<uint8_t> AESGCMEncrypt(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& plaintext);

// AES-256-GCM 解密，输入格式为 nonce || ciphertext || tag
std::vector<uint8_t> AESGCMDecrypt(const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& ciphertext);

} // namespace crypto
} // namespace gs
