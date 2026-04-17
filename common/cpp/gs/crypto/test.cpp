#include "aes_gcm.hpp"
#include "hmac.hpp"
#include <iostream>
#include <vector>

int main() {
    using namespace gs::crypto;

    // AES-GCM round trip
    std::vector<uint8_t> key(32);
    for (size_t i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i);
    std::vector<uint8_t> plaintext = {'h', 'e', 'l', 'l', 'o'};

    auto ciphertext = AESGCMEncrypt(key, plaintext);
    auto decrypted = AESGCMDecrypt(key, ciphertext);

    if (decrypted != plaintext) {
        std::cerr << "AES-GCM round trip FAILED" << std::endl;
        return 1;
    }
    std::cout << "AES-GCM round trip PASSED" << std::endl;

    // AES-GCM wrong key
    key[0] ^= 0xFF;
    try {
        AESGCMDecrypt(key, ciphertext);
        std::cerr << "AES-GCM wrong key test FAILED" << std::endl;
        return 1;
    } catch (...) {
        std::cout << "AES-GCM wrong key PASSED" << std::endl;
    }

    // HMAC-SHA256
    std::vector<uint8_t> hmacKey = {'k', 'e', 'y'};
    std::vector<uint8_t> data = {'d', 'a', 't', 'a'};
    std::string sig = HMACSHA256(hmacKey, data);
    if (sig.empty()) {
        std::cerr << "HMAC-SHA256 FAILED" << std::endl;
        return 1;
    }
    if (!VerifyHMACSHA256(hmacKey, data, sig)) {
        std::cerr << "HMAC-SHA256 verify FAILED" << std::endl;
        return 1;
    }
    std::cout << "HMAC-SHA256 PASSED" << std::endl;

    std::cout << "All crypto tests PASSED" << std::endl;
    return 0;
}
