package crypto

import (
	"crypto/rand"
	"crypto/subtle"
	"encoding/base64"
	"fmt"
	"strings"

	"golang.org/x/crypto/argon2"
)

// argon2idParams 默认参数配置
// time=3, memory=64MB, threads=4, keyLen=32, saltLen=16
const (
	argon2Time    = 3
	argon2Memory  = 64 * 1024
	argon2Threads = 4
	argon2KeyLen  = 32
	argon2SaltLen = 16
)

// HashPasswordArgon2id 使用 Argon2id 哈希密码
// 返回格式: $argon2id$<base64(salt)>$<base64(hash)>
func HashPasswordArgon2id(password string) (string, error) {
	salt := make([]byte, argon2SaltLen)
	if _, err := rand.Read(salt); err != nil {
		return "", fmt.Errorf("generate salt failed: %w", err)
	}
	hash := argon2.IDKey([]byte(password), salt, argon2Time, argon2Memory, argon2Threads, argon2KeyLen)
	return fmt.Sprintf("$argon2id$%s$%s",
		base64.RawStdEncoding.EncodeToString(salt),
		base64.RawStdEncoding.EncodeToString(hash),
	), nil
}

// VerifyPasswordArgon2id 验证 Argon2id 密码
func VerifyPasswordArgon2id(password, encodedHash string) (bool, error) {
	parts := strings.Split(encodedHash, "$")
	if len(parts) != 4 || parts[1] != "argon2id" {
		return false, fmt.Errorf("invalid argon2id hash format")
	}
	salt, err := base64.RawStdEncoding.DecodeString(parts[2])
	if err != nil {
		return false, fmt.Errorf("decode salt failed: %w", err)
	}
	expectedHash, err := base64.RawStdEncoding.DecodeString(parts[3])
	if err != nil {
		return false, fmt.Errorf("decode hash failed: %w", err)
	}
	hash := argon2.IDKey([]byte(password), salt, argon2Time, argon2Memory, argon2Threads, uint32(len(expectedHash)))
	return subtle.ConstantTimeCompare(hash, expectedHash) == 1, nil
}

// IsArgon2idHash 判断密码哈希是否是 Argon2id 格式
func IsArgon2idHash(hash string) bool {
	return strings.HasPrefix(hash, "$argon2id$")
}
