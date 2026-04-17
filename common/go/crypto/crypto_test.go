package crypto

import (
	"bytes"
	"testing"
)

func TestAESGCMRoundTrip(t *testing.T) {
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i)
	}
	plaintext := []byte("hello gmaker crypto")

	ciphertext, err := AESGCMEncrypt(key, plaintext)
	if err != nil {
		t.Fatalf("encrypt failed: %v", err)
	}
	if bytes.Equal(ciphertext, plaintext) {
		t.Fatal("ciphertext equals plaintext")
	}

	decrypted, err := AESGCMDecrypt(key, ciphertext)
	if err != nil {
		t.Fatalf("decrypt failed: %v", err)
	}
	if !bytes.Equal(decrypted, plaintext) {
		t.Fatalf("decrypted mismatch: got %s, want %s", decrypted, plaintext)
	}
}

func TestAESGCMDecryptWrongKey(t *testing.T) {
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i)
	}
	plaintext := []byte("secret")
	ciphertext, _ := AESGCMEncrypt(key, plaintext)

	key[0] ^= 0xFF
	if _, err := AESGCMDecrypt(key, ciphertext); err == nil {
		t.Fatal("expected decrypt error with wrong key")
	}
}

func TestHMACSHA256(t *testing.T) {
	key := []byte("my-secret-key")
	data := []byte("message to sign")

	sig := HMACSHA256(key, data)
	if sig == "" {
		t.Fatal("signature empty")
	}

	if !VerifyHMACSHA256(key, data, sig) {
		t.Fatal("signature verification failed")
	}

	if VerifyHMACSHA256(key, data, sig+"bad") {
		t.Fatal("signature verification should fail with bad sig")
	}
}

func TestHMACSHA256CrossValidation(t *testing.T) {
	key := []byte("cross-validation-key")
	data := []byte("cross-validation-data")

	sig1 := HMACSHA256(key, data)
	sig2 := HMACSHA256(key, data)
	if sig1 != sig2 {
		t.Fatalf("same input produced different signatures: %s vs %s", sig1, sig2)
	}
}
