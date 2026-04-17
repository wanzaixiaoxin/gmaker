#pragma once

#include <string>
#include <vector>

namespace gs {
namespace crypto {

// HMAC-SHA256 计算，返回十六进制字符串
std::string HMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

// 验证 HMAC-SHA256
bool VerifyHMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data,
                      const std::string& expectedHex);

} // namespace crypto
} // namespace gs
