#include "crypto/oqs/oqs_signature.hpp"

#include <string>

#include "tcc/crypto/crypto_error.hpp"

namespace tcc::crypto::oqs {
namespace {

const char* oqs_name(const protocol::AlgorithmId algorithm_id) {
    switch (algorithm_id) {
        case protocol::AlgorithmId::falcon_512:
            return OQS_SIG_alg_falcon_512;
        case protocol::AlgorithmId::falcon_1024:
            return OQS_SIG_alg_falcon_1024;
        default:
            throw CryptoError(
                CryptoErrorCode::unsupported_algorithm,
                "liboqs adapter does not support the requested algorithm");
    }
}

}  // namespace

void OqsSignature::Deleter::operator()(OQS_SIG* value) const noexcept {
    OQS_SIG_free(value);
}

OqsSignature::OqsSignature(const protocol::AlgorithmId algorithm_id)
    : algorithm_id_(algorithm_id), implementation_(OQS_SIG_new(oqs_name(algorithm_id))) {
    if (!implementation_) {
        throw CryptoError(CryptoErrorCode::provider_unavailable,
                          std::string(oqs_name(algorithm_id)) +
                              " is not enabled in liboqs");
    }
}

protocol::AlgorithmId OqsSignature::id() const noexcept {
    return algorithm_id_;
}

std::string_view OqsSignature::name() const noexcept {
    return implementation_->method_name;
}

std::string_view OqsSignature::provider() const noexcept {
    return "liboqs";
}

KeyPair OqsSignature::generate_key_pair() const {
    KeyPair result{Bytes(implementation_->length_public_key),
                   Bytes(implementation_->length_secret_key)};
    if (OQS_SIG_keypair(implementation_.get(), result.public_key.data(),
                        result.private_key.data()) != OQS_SUCCESS) {
        throw CryptoError(CryptoErrorCode::key_generation_failed,
                          std::string("failed to generate ") +
                              implementation_->method_name + " key pair");
    }
    return result;
}

SignatureResult OqsSignature::sign(const Bytes& message,
                                   const Bytes& private_key) const {
    if (private_key.size() != implementation_->length_secret_key) {
        throw CryptoError(CryptoErrorCode::invalid_key,
                          std::string("invalid ") + implementation_->method_name +
                              " private key length");
    }

    SignatureResult result{Bytes(implementation_->length_signature)};
    std::size_t actual_size = result.signature.size();
    if (OQS_SIG_sign(implementation_.get(), result.signature.data(), &actual_size,
                     message.data(), message.size(), private_key.data()) !=
        OQS_SUCCESS) {
        throw CryptoError(CryptoErrorCode::signing_failed,
                          std::string("failed to create ") +
                              implementation_->method_name + " signature");
    }
    result.signature.resize(actual_size);
    return result;
}

bool OqsSignature::verify(const Bytes& message,
                          const Bytes& signature,
                          const Bytes& public_key) const {
    if (public_key.size() != implementation_->length_public_key) {
        throw CryptoError(CryptoErrorCode::invalid_key,
                          std::string("invalid ") + implementation_->method_name +
                              " public key length");
    }
    if (signature.empty() ||
        signature.size() > implementation_->length_signature) {
        return false;
    }

    return OQS_SIG_verify(implementation_.get(), message.data(), message.size(),
                          signature.data(), signature.size(), public_key.data()) ==
           OQS_SUCCESS;
}

}  // namespace tcc::crypto::oqs
