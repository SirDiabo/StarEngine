#include "AuthChallenge.h"
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace {

std::string ToHex(const unsigned char* data, size_t len) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out += digits[(data[i] >> 4) & 0xF];
        out += digits[data[i] & 0xF];
    }
    return out;
}

} // namespace

namespace AuthChallenge {

std::string GenerateNonceHex() {
    unsigned char buf[16];
    if (RAND_bytes(buf, sizeof(buf)) != 1) return "";
    return ToHex(buf, sizeof(buf));
}

std::string ComputeHmacHex(const std::string& password, const std::string& nonceHex) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int outLen = 0;
    HMAC(EVP_sha256(), password.data(), (int)password.size(),
         (const unsigned char*)nonceHex.data(), nonceHex.size(), out, &outLen);
    return ToHex(out, outLen);
}

} // namespace AuthChallenge