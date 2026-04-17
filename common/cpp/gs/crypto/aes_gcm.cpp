#include "aes_gcm.hpp"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#ifndef BCRYPT_AES_GCM_ALGORITHM
#define BCRYPT_AES_GCM_ALGORITHM L"AESGCM"
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace gs {
namespace crypto {

std::vector<uint8_t> AESGCMEncrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& plaintext) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes for AES-256");
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_GCM_ALGORITHM, nullptr, 0))) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    }

    DWORD cbResult = 0;
    DWORD cbKeyObject = 0;
    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbKeyObject, sizeof(DWORD), &cbResult, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGetProperty OBJECT_LENGTH failed");
    }

    DWORD cbBlockLen = 0;
    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (PBYTE)&cbBlockLen, sizeof(DWORD), &cbResult, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGetProperty BLOCK_LENGTH failed");
    }

    std::vector<uint8_t> keyObject(cbKeyObject);
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObject.data(), cbKeyObject, (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGenerateSymmetricKey failed");
    }

    // nonce (IV)
    constexpr size_t kNonceSize = 12;
    std::vector<uint8_t> nonce(kNonceSize);
    if (!NT_SUCCESS(BCryptGenRandom(nullptr, nonce.data(), (ULONG)nonce.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGenRandom failed");
    }

    // auth tag size = 16 bytes for GCM
    constexpr size_t kTagSize = 16;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = (ULONG)nonce.size();
    authInfo.pbTag = nullptr;
    authInfo.cbTag = 0;

    ULONG cbCipherText = 0;
    std::vector<uint8_t> ciphertext(plaintext.size() + kTagSize);
    if (!NT_SUCCESS(BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), &authInfo,
                                   nullptr, 0, ciphertext.data(), (ULONG)ciphertext.size(), &cbCipherText, 0))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptEncrypt (size) failed");
    }

    authInfo.pbTag = ciphertext.data() + plaintext.size();
    authInfo.cbTag = kTagSize;
    if (!NT_SUCCESS(BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), &authInfo,
                                   nullptr, 0, ciphertext.data(), (ULONG)ciphertext.size(), &cbCipherText, 0))) {
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptEncrypt failed");
    }

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    // format: nonce || ciphertext || tag
    std::vector<uint8_t> result;
    result.reserve(nonce.size() + ciphertext.size());
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

std::vector<uint8_t> AESGCMDecrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& data) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes for AES-256");
    }

    constexpr size_t kNonceSize = 12;
    constexpr size_t kTagSize = 16;
    if (data.size() < kNonceSize + kTagSize) {
        throw std::invalid_argument("ciphertext too short");
    }

    std::vector<uint8_t> nonce(data.begin(), data.begin() + kNonceSize);
    std::vector<uint8_t> ciphertext(data.begin() + kNonceSize, data.end());
    size_t plaintextSize = ciphertext.size() - kTagSize;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_GCM_ALGORITHM, nullptr, 0))) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
    }

    DWORD cbResult = 0;
    DWORD cbKeyObject = 0;
    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbKeyObject, sizeof(DWORD), &cbResult, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGetProperty OBJECT_LENGTH failed");
    }

    std::vector<uint8_t> keyObject(cbKeyObject);
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!NT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, keyObject.data(), cbKeyObject, (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("BCryptGenerateSymmetricKey failed");
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = (ULONG)nonce.size();
    authInfo.pbTag = ciphertext.data() + plaintextSize;
    authInfo.cbTag = kTagSize;

    std::vector<uint8_t> plaintext(plaintextSize);
    ULONG cbPlaintext = 0;
    NTSTATUS status = BCryptDecrypt(hKey, ciphertext.data(), (ULONG)plaintextSize, &authInfo,
                                    nullptr, 0, plaintext.data(), (ULONG)plaintext.size(), &cbPlaintext, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!NT_SUCCESS(status)) {
        throw std::runtime_error("BCryptDecrypt failed (invalid ciphertext or tag)");
    }

    return plaintext;
}

} // namespace crypto
} // namespace gs

#else

// Linux/macOS fallback using OpenSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

namespace gs {
namespace crypto {

std::vector<uint8_t> AESGCMEncrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& plaintext) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes for AES-256");
    }

    constexpr size_t kNonceSize = 12;
    constexpr size_t kTagSize = 16;
    std::vector<uint8_t> nonce(kNonceSize);
    if (RAND_bytes(nonce.data(), (int)nonce.size()) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)kNonceSize, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CIPHER_CTX_ctrl failed");
    }
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex key/iv failed");
    }

    std::vector<uint8_t> ciphertext(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), (int)plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptUpdate failed");
    }
    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    ciphertext_len += len;

    std::vector<uint8_t> tag(kTagSize);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)kTagSize, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CTRL_GCM_GET_TAG failed");
    }
    EVP_CIPHER_CTX_free(ctx);

    std::vector<uint8_t> result;
    result.reserve(nonce.size() + ciphertext.size() + tag.size());
    result.insert(result.end(), nonce.begin(), nonce.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag.begin(), tag.end());
    return result;
}

std::vector<uint8_t> AESGCMDecrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& data) {
    if (key.size() != 32) {
        throw std::invalid_argument("key must be 32 bytes for AES-256");
    }

    constexpr size_t kNonceSize = 12;
    constexpr size_t kTagSize = 16;
    if (data.size() < kNonceSize + kTagSize) {
        throw std::invalid_argument("ciphertext too short");
    }

    std::vector<uint8_t> nonce(data.begin(), data.begin() + kNonceSize);
    std::vector<uint8_t> ciphertext(data.begin() + kNonceSize, data.end() - kTagSize);
    std::vector<uint8_t> tag(data.end() - kTagSize, data.end());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex failed");
    }
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)kNonceSize, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CIPHER_CTX_ctrl failed");
    }
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptInit_ex key/iv failed");
    }

    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), (int)ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_DecryptUpdate failed");
    }
    int plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)kTagSize, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_CTRL_GCM_SET_TAG failed");
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("AES-GCM tag verification failed");
    }
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(plaintext_len);
    return plaintext;
}

} // namespace crypto
} // namespace gs

#endif
