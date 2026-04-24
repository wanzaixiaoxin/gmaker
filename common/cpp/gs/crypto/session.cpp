#include "session.hpp"
#include "aes_gcm.hpp"
#include "hmac.hpp"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif
#else
#include <openssl/rand.h>
#endif

namespace gs {
namespace crypto {

std::vector<uint8_t> DeriveSessionKey(const std::vector<uint8_t>& masterKey,
                                       const std::vector<uint8_t>& clientRandom) {
    if (masterKey.size() != 32) {
        throw std::invalid_argument("master key must be 32 bytes");
    }
    std::vector<uint8_t> label = {'g','m','a','k','e','r','-','s','e','s','s','i','o','n','-','v','1'};
    std::vector<uint8_t> data;
    data.reserve(label.size() + clientRandom.size());
    data.insert(data.end(), label.begin(), label.end());
    data.insert(data.end(), clientRandom.begin(), clientRandom.end());
    return HMACSHA256Raw(masterKey, data);
}

std::vector<uint8_t> DeriveSessionKey(const std::vector<uint8_t>& masterKey,
                                       const std::vector<uint8_t>& clientRandom,
                                       const std::vector<uint8_t>& serverRandom) {
    if (masterKey.size() != 32) {
        throw std::invalid_argument("master key must be 32 bytes");
    }
    std::vector<uint8_t> label = {'g','m','a','k','e','r','-','s','e','s','s','i','o','n','-','v','1'};
    std::vector<uint8_t> data;
    data.reserve(label.size() + clientRandom.size() + serverRandom.size());
    data.insert(data.end(), label.begin(), label.end());
    data.insert(data.end(), clientRandom.begin(), clientRandom.end());
    data.insert(data.end(), serverRandom.begin(), serverRandom.end());
    return HMACSHA256Raw(masterKey, data);
}

std::vector<uint8_t> RandomBytes(size_t n) {
    std::vector<uint8_t> buf(n);
#ifdef _WIN32
    if (!NT_SUCCESS(BCryptGenRandom(nullptr, buf.data(), (ULONG)buf.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
#else
    if (RAND_bytes(buf.data(), (int)buf.size()) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
#endif
    return buf;
}

std::vector<uint8_t> EncryptPacketPayload(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& payload) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes");
    }
    return AESGCMEncrypt(key, payload);
}

std::vector<uint8_t> DecryptPacketPayload(const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& encryptedPayload) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes");
    }
    return AESGCMDecrypt(key, encryptedPayload);
}

} // namespace crypto
} // namespace gs
