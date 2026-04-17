package crypto

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"fmt"
)

// DeriveSessionKey 使用 HMAC-SHA256 派生 session key
// masterKey 必须为 32 字节
func DeriveSessionKey(masterKey, clientRandom, serverRandom []byte) ([]byte, error) {
	if len(masterKey) != 32 {
		return nil, fmt.Errorf("master key must be 32 bytes")
	}
	mac := hmac.New(sha256.New, masterKey)
	mac.Write([]byte("gmaker-session-v1"))
	mac.Write(clientRandom)
	mac.Write(serverRandom)
	return mac.Sum(nil), nil
}

// RandomBytes 生成 n 字节随机数据
func RandomBytes(n int) ([]byte, error) {
	b := make([]byte, n)
	if _, err := rand.Read(b); err != nil {
		return nil, err
	}
	return b, nil
}

// EncryptPacketPayload 使用 AES-256-GCM 加密 packet payload
func EncryptPacketPayload(key, payload []byte) ([]byte, error) {
	if len(key) != 32 {
		return nil, fmt.Errorf("key must be 32 bytes")
	}
	return AESGCMEncrypt(key, payload)
}

// DecryptPacketPayload 解密 AES-256-GCM 加密的 packet payload
func DecryptPacketPayload(key, encryptedPayload []byte) ([]byte, error) {
	if len(key) != 32 {
		return nil, fmt.Errorf("key must be 32 bytes")
	}
	return AESGCMDecrypt(key, encryptedPayload)
}
