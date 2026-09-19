#include "tcc/crypto/factory.hpp"

#include "crypto/adapters.hpp"
#include "tcc/crypto/error.hpp"

#include <memory>

namespace tcc {

bool is_supported_algorithm(const Algorithm algorithm) noexcept {
    switch (algorithm) {
        case Algorithm::ml_dsa_44:
        case Algorithm::ml_dsa_65:
        case Algorithm::ml_dsa_87:
        case Algorithm::falcon_512:
        case Algorithm::falcon_1024:
        case Algorithm::rsa_pss:
        case Algorithm::ed25519:
            return true;
    }
    return false;
}

std::unique_ptr<SignatureAlgorithm> create_signature_algorithm(
    const Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::ml_dsa_44:
        case Algorithm::ml_dsa_65:
        case Algorithm::ml_dsa_87:
            return std::make_unique<MlDsa>(algorithm);
        case Algorithm::falcon_512:
        case Algorithm::falcon_1024:
            return std::make_unique<Falcon>(algorithm);
        case Algorithm::rsa_pss:
            return std::make_unique<RsaPss>();
        case Algorithm::ed25519:
            return std::make_unique<Ed25519>();
    }
    throw CryptoError(
        CryptoErrorCode::unsupported_algorithm,
        "signature algorithm is not supported");
}

}  // namespace tcc
