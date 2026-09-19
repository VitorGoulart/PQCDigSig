#include "crypto/openssl_helpers.hpp"

#include <string>
#include <utility>

#include <openssl/err.h>

namespace tcc::openssl_detail {

void EvpPkeyDeleter::operator()(EVP_PKEY* value) const noexcept {
    EVP_PKEY_free(value);
}

void EvpPkeyContextDeleter::operator()(EVP_PKEY_CTX* value) const noexcept {
    EVP_PKEY_CTX_free(value);
}

void EvpMessageDigestContextDeleter::operator()(EVP_MD_CTX* value) const noexcept {
    EVP_MD_CTX_free(value);
}

void clear_error_queue() noexcept {
    ERR_clear_error();
}

void throw_openssl_error(
    const CryptoErrorCode code,
    const std::string_view operation) {
    std::string message(operation);
    const unsigned long error = ERR_get_error();
    if (error != 0UL) {
        char buffer[256]{};
        ERR_error_string_n(error, buffer, sizeof(buffer));
        message += ": ";
        message += buffer;
    }
    throw CryptoError(code, std::move(message));
}

}  // namespace tcc::openssl_detail
