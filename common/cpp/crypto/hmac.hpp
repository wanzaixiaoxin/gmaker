#pragma once

#include <string>
#include <vector>

namespace gs {
namespace crypto {

// HMAC-SHA256 计算，返回十六进制字符串
std::string HMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

// HMAC-SHA256 计算，返回原始 32 字节二进制
std::vector<uint8_t> HMACSHA256Raw(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

// 验证 HMAC-SHA256
bool VerifyHMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data,
                      const std::string& expectedHex);

} // namespace crypto
} // namespace gs
