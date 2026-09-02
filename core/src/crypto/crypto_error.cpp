#include "tcc/crypto/crypto_error.hpp"

#include <utility>

namespace tcc::crypto {

CryptoError::CryptoError(const CryptoErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

CryptoErrorCode CryptoError::code() const noexcept {
    return code_;
}

}  // namespace tcc::crypto
