#include "hmac.hpp"
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace gs {
namespace crypto {

std::vector<uint8_t> HMACSHA256Raw(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return {};
    }

    DWORD cbHash = 0;
    DWORD cbResult = 0;
    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbResult, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    DWORD cbHashObject = 0;
    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbResult, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<uint8_t> hashObject(cbHashObject);
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    if (!NT_SUCCESS(BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    std::vector<uint8_t> hash(cbHash);
    if (!NT_SUCCESS(BCryptFinishHash(hHash, hash.data(), cbHash, 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hash;
}

std::string HMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash = HMACSHA256Raw(key, data);
    std::ostringstream oss;
    for (auto b : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

bool VerifyHMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data,
                      const std::string& expectedHex) {
    std::string computed = HMACSHA256(key, data);
    if (computed.size() != expectedHex.size()) return false;
    volatile uint8_t result = 0;
    for (size_t i = 0; i < computed.size(); ++i) {
        result |= (computed[i] ^ expectedHex[i]);
    }
    return result == 0;
}

} // namespace crypto
} // namespace gs

#else

#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace gs {
namespace crypto {

std::vector<uint8_t> HMACSHA256Raw(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    unsigned int len = 0;
    unsigned char hash[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), key.data(), (int)key.size(), data.data(), data.size(), hash, &len);
    return std::vector<uint8_t>(hash, hash + len);
}

std::string HMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash = HMACSHA256Raw(key, data);
    std::ostringstream oss;
    for (auto b : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

bool VerifyHMACSHA256(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data,
                      const std::string& expectedHex) {
    std::string computed = HMACSHA256(key, data);
    if (computed.size() != expectedHex.size()) return false;
    volatile uint8_t result = 0;
    for (size_t i = 0; i < computed.size(); ++i) {
        result |= (computed[i] ^ expectedHex[i]);
    }
    return result == 0;
}

} // namespace crypto
} // namespace gs

#endif
