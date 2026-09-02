#include "tcc/crypto/algorithm_factory.hpp"

#include "crypto/openssl/ed25519.hpp"
#include "crypto/openssl/ml_dsa.hpp"
#include "crypto/openssl/rsa_pss.hpp"
#include "crypto/oqs/oqs_signature.hpp"
#include "tcc/crypto/crypto_error.hpp"

namespace tcc::crypto {

bool is_signature_algorithm_implemented(
    const protocol::AlgorithmId algorithm_id) noexcept {
    switch (algorithm_id) {
        case protocol::AlgorithmId::ml_dsa_44:
        case protocol::AlgorithmId::ml_dsa_65:
        case protocol::AlgorithmId::ml_dsa_87:
        case protocol::AlgorithmId::falcon_512:
        case protocol::AlgorithmId::falcon_1024:
        case protocol::AlgorithmId::rsa_pss:
        case protocol::AlgorithmId::ed25519:
            return true;
        default:
            return false;
    }
}

std::unique_ptr<SignatureAlgorithm> create_signature_algorithm(
    const protocol::AlgorithmId algorithm_id) {
    switch (algorithm_id) {
        case protocol::AlgorithmId::ml_dsa_44:
        case protocol::AlgorithmId::ml_dsa_65:
        case protocol::AlgorithmId::ml_dsa_87:
            return std::make_unique<openssl::MlDsa>(algorithm_id);
        case protocol::AlgorithmId::falcon_512:
        case protocol::AlgorithmId::falcon_1024:
            return std::make_unique<oqs::OqsSignature>(algorithm_id);
        case protocol::AlgorithmId::rsa_pss:
            return std::make_unique<openssl::RsaPss>();
        case protocol::AlgorithmId::ed25519:
            return std::make_unique<openssl::Ed25519>();
        default:
            throw CryptoError(CryptoErrorCode::unsupported_algorithm,
                              "signature algorithm is not implemented");
    }
}

}  // namespace tcc::crypto
