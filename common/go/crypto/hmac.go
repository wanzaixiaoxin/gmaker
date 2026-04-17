package crypto

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
)

// HMACSHA256 计算 HMAC-SHA256，返回十六进制字符串
func HMACSHA256(key, data []byte) string {
	mac := hmac.New(sha256.New, key)
	mac.Write(data)
	return hex.EncodeToString(mac.Sum(nil))
}

// VerifyHMACSHA256 验证 HMAC-SHA256
func VerifyHMACSHA256(key, data []byte, expectedHex string) bool {
	return hmac.Equal([]byte(HMACSHA256(key, data)), []byte(expectedHex))
}
