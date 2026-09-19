#include "tcc/crypto.hpp"

#include <utility>

namespace tcc {

std::string_view algorithm_name(const Algorithm algorithm) noexcept {
    switch (algorithm) {
        case Algorithm::ml_dsa_44:
            return "ML-DSA-44";
        case Algorithm::ml_dsa_65:
            return "ML-DSA-65";
        case Algorithm::ml_dsa_87:
            return "ML-DSA-87";
        case Algorithm::falcon_512:
            return "Falcon-512";
        case Algorithm::falcon_1024:
            return "Falcon-1024";
        case Algorithm::rsa_pss:
            return "RSASSA-PSS";
        case Algorithm::ed25519:
            return "Ed25519";
    }
    return "unknown";
}

CryptoProvider algorithm_provider(const Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::ml_dsa_44:
        case Algorithm::ml_dsa_65:
        case Algorithm::ml_dsa_87:
        case Algorithm::rsa_pss:
        case Algorithm::ed25519:
            return CryptoProvider::openssl;
        case Algorithm::falcon_512:
        case Algorithm::falcon_1024:
            return CryptoProvider::liboqs;
    }
    throw CryptoError(
        CryptoErrorCode::unsupported_algorithm,
        "unsupported signature algorithm");
}

std::string_view provider_name(const CryptoProvider provider) noexcept {
    switch (provider) {
        case CryptoProvider::openssl:
            return "OpenSSL";
        case CryptoProvider::liboqs:
            return "liboqs";
    }
    return "unknown";
}

CryptoError::CryptoError(const CryptoErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

CryptoErrorCode CryptoError::code() const noexcept {
    return code_;
}

}  // namespace tcc
