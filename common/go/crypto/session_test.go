package crypto

import (
	"bytes"
	"testing"
)

func TestDeriveSessionKey(t *testing.T) {
	masterKey := make([]byte, 32)
	for i := range masterKey {
		masterKey[i] = byte(i)
	}
	clientRandom := []byte("client-random-16")
	serverRandom := []byte("server-random-16")

	key1, err := DeriveSessionKey(masterKey, clientRandom, serverRandom)
	if err != nil {
		t.Fatalf("derive failed: %v", err)
	}
	if len(key1) != 32 {
		t.Fatalf("expected 32 bytes key, got %d", len(key1))
	}

	key2, err := DeriveSessionKey(masterKey, clientRandom, serverRandom)
	if err != nil {
		t.Fatalf("derive failed: %v", err)
	}
	if !bytes.Equal(key1, key2) {
		t.Fatal("deterministic derivation mismatch")
	}
}

func TestEncryptDecryptPacketPayload(t *testing.T) {
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i)
	}
	payload := []byte("hello, encrypted world!")

	enc, err := EncryptPacketPayload(key, payload)
	if err != nil {
		t.Fatalf("encrypt failed: %v", err)
	}
	if bytes.Equal(enc, payload) {
		t.Fatal("encrypted payload should differ from plaintext")
	}

	dec, err := DecryptPacketPayload(key, enc)
	if err != nil {
		t.Fatalf("decrypt failed: %v", err)
	}
	if !bytes.Equal(dec, payload) {
		t.Fatalf("decrypt mismatch: got %s, want %s", dec, payload)
	}
}
