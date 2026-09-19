#include "crypto/adapters.hpp"

#include "tcc/crypto/error.hpp"

#include <string>

#include <oqs/oqs.h>

namespace tcc {
namespace {

const char* falcon_oqs_name(const Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::falcon_512:
            return OQS_SIG_alg_falcon_512;
        case Algorithm::falcon_1024:
            return OQS_SIG_alg_falcon_1024;
        default:
            throw CryptoError(
                CryptoErrorCode::unsupported_algorithm,
                "liboqs Falcon adapter does not support the requested algorithm");
    }
}

}  // namespace

void Falcon::Deleter::operator()(OQS_SIG* value) const noexcept {
    OQS_SIG_free(value);
}

Falcon::Falcon(const Algorithm algorithm)
    : algorithm_(algorithm), implementation_(OQS_SIG_new(falcon_oqs_name(algorithm))) {
    if (!implementation_) {
        throw CryptoError(
            CryptoErrorCode::provider_unavailable,
            std::string(falcon_oqs_name(algorithm)) +
                " is not enabled in liboqs");
    }
}

Falcon::~Falcon() = default;

Algorithm Falcon::id() const noexcept {
    return algorithm_;
}

std::size_t Falcon::public_key_size() const noexcept {
    return implementation_->length_public_key;
}

std::size_t Falcon::maximum_signature_size() const noexcept {
    return implementation_->length_signature;
}

KeyPair Falcon::generate_key_pair() const {
    KeyPair result{
        Bytes(implementation_->length_public_key),
        Bytes(implementation_->length_secret_key),
    };
    if (OQS_SIG_keypair(
            implementation_.get(), result.public_key.data(),
            result.private_key.data()) != OQS_SUCCESS) {
        throw CryptoError(
            CryptoErrorCode::key_generation_failed,
            std::string("failed to generate ") + implementation_->method_name +
                " key pair");
    }
    return result;
}

Bytes Falcon::sign(const Bytes& payload, const Bytes& private_key) const {
    if (private_key.size() != implementation_->length_secret_key) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            std::string("invalid ") + implementation_->method_name +
                " private key length");
    }

    Bytes signature(implementation_->length_signature);
    std::size_t actual_size = signature.size();
    if (OQS_SIG_sign(
            implementation_.get(), signature.data(), &actual_size,
            payload.data(), payload.size(), private_key.data()) != OQS_SUCCESS) {
        throw CryptoError(
            CryptoErrorCode::signing_failed,
            std::string("failed to create ") + implementation_->method_name +
                " signature");
    }
    signature.resize(actual_size);
    return signature;
}

bool Falcon::verify(
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key) const {
    if (public_key.size() != implementation_->length_public_key) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            std::string("invalid ") + implementation_->method_name +
                " public key length");
    }
    if (signature.empty() ||
        signature.size() > implementation_->length_signature) {
        return false;
    }

    return OQS_SIG_verify(
               implementation_.get(), payload.data(), payload.size(),
               signature.data(), signature.size(), public_key.data()) ==
           OQS_SUCCESS;
}

}  // namespace tcc
