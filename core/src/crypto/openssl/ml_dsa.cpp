#include "crypto/openssl/ml_dsa.hpp"

#include <cstddef>
#include <cstdint>

#include <openssl/core_names.h>
#include <openssl/params.h>

#include "crypto/openssl/openssl_helpers.hpp"
#include "tcc/crypto/crypto_error.hpp"

namespace tcc::crypto::openssl {
namespace {

struct MlDsaParameters {
    std::string_view name;
    std::size_t public_key_bytes;
    std::size_t private_key_bytes;
    std::size_t signature_bytes;
};

MlDsaParameters parameters_for(const protocol::AlgorithmId algorithm_id) {
    switch (algorithm_id) {
        case protocol::AlgorithmId::ml_dsa_44:
            return {"ML-DSA-44", 1312, 2560, 2420};
        case protocol::AlgorithmId::ml_dsa_65:
            return {"ML-DSA-65", 1952, 4032, 3309};
        case protocol::AlgorithmId::ml_dsa_87:
            return {"ML-DSA-87", 2592, 4896, 4627};
        default:
            throw CryptoError(CryptoErrorCode::unsupported_algorithm,
                              "OpenSSL ML-DSA adapter does not support the requested algorithm");
    }
}

Bytes export_raw_key(EVP_PKEY* key,
                     const bool public_component,
                     const std::string_view algorithm_name,
                     const CryptoErrorCode error_code) {
    std::size_t size = 0;
    const int query_result = public_component
                                 ? EVP_PKEY_get_raw_public_key(key, nullptr, &size)
                                 : EVP_PKEY_get_raw_private_key(key, nullptr, &size);
    if (query_result != 1 || size == 0U) {
        detail::throw_openssl_error(
            error_code,
            std::string("failed to query ") + std::string(algorithm_name) +
                " key size");
    }

    Bytes output(size);
    const int export_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, output.data(), &size)
            : EVP_PKEY_get_raw_private_key(key, output.data(), &size);
    if (export_result != 1) {
        detail::throw_openssl_error(
            error_code,
            std::string("failed to export ") + std::string(algorithm_name) +
                " key");
    }
    output.resize(size);
    return output;
}

detail::EvpPkey import_raw_key(const Bytes& encoded_key,
                              const bool public_component,
                              const std::string_view algorithm_name,
                              const std::size_t expected_size) {
    if (encoded_key.size() != expected_size) {
        throw CryptoError(CryptoErrorCode::invalid_key,
                          std::string("invalid ") + std::string(algorithm_name) +
                              " key length");
    }

    detail::clear_error_queue();
    detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, algorithm_name.data(), nullptr)};
    if (!context || EVP_PKEY_fromdata_init(context.get()) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::provider_unavailable,
                                    std::string("failed to initialize ") +
                                        std::string(algorithm_name) +
                                        " key import");
    }

    auto* mutable_data = const_cast<std::uint8_t*>(encoded_key.data());
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_octet_string(
            public_component ? OSSL_PKEY_PARAM_PUB_KEY
                             : OSSL_PKEY_PARAM_PRIV_KEY,
            mutable_data,
            encoded_key.size()),
        OSSL_PARAM_construct_end(),
    };

    EVP_PKEY* raw_key = nullptr;
    const int selection = public_component ? EVP_PKEY_PUBLIC_KEY
                                           : EVP_PKEY_PRIVATE_KEY;
    if (EVP_PKEY_fromdata(context.get(), &raw_key, selection, parameters) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::invalid_key,
                                    std::string("failed to import ") +
                                        std::string(algorithm_name) + " key");
    }
    return detail::EvpPkey{raw_key};
}

}  // namespace

MlDsa::MlDsa(const protocol::AlgorithmId algorithm_id)
    : algorithm_id_(algorithm_id),
      algorithm_name_(),
      public_key_bytes_(0),
      private_key_bytes_(0),
      signature_bytes_(0) {
    const MlDsaParameters parameters = parameters_for(algorithm_id);
    algorithm_name_ = parameters.name;
    public_key_bytes_ = parameters.public_key_bytes;
    private_key_bytes_ = parameters.private_key_bytes;
    signature_bytes_ = parameters.signature_bytes;
}

protocol::AlgorithmId MlDsa::id() const noexcept {
    return algorithm_id_;
}

std::string_view MlDsa::name() const noexcept {
    return algorithm_name_;
}

std::string_view MlDsa::provider() const noexcept {
    return "OpenSSL";
}

KeyPair MlDsa::generate_key_pair() const {
    detail::clear_error_queue();
    detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, algorithm_name_.data(), nullptr)};
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::provider_unavailable,
                                    std::string("failed to initialize ") +
                                        std::string(algorithm_name_) +
                                        " key generation");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_generate(context.get(), &raw_key) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::key_generation_failed,
                                    std::string("failed to generate ") +
                                        std::string(algorithm_name_) +
                                        " key pair");
    }
    detail::EvpPkey key{raw_key};

    KeyPair result;
    result.public_key =
        export_raw_key(key.get(), true, algorithm_name_,
                       CryptoErrorCode::key_generation_failed);
    result.private_key =
        export_raw_key(key.get(), false, algorithm_name_,
                       CryptoErrorCode::key_generation_failed);
    return result;
}

SignatureResult MlDsa::sign(const Bytes& message,
                            const Bytes& private_key) const {
    detail::EvpPkey key = import_raw_key(private_key, false, algorithm_name_,
                                         private_key_bytes_);
    detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    std::string("failed to allocate ") +
                                        std::string(algorithm_name_) +
                                        " signing context");
    }

    if (EVP_DigestSignInit_ex(context.get(), nullptr, nullptr, nullptr, nullptr,
                              key.get(), nullptr) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    std::string("failed to initialize ") +
                                        std::string(algorithm_name_) + " signing");
    }

    std::size_t size = 0;
    if (EVP_DigestSign(context.get(), nullptr, &size, message.data(),
                       message.size()) != 1 ||
        size == 0U) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    std::string("failed to query ") +
                                        std::string(algorithm_name_) +
                                        " signature size");
    }

    SignatureResult result{Bytes(size)};
    if (EVP_DigestSign(context.get(), result.signature.data(), &size,
                       message.data(), message.size()) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    std::string("failed to create ") +
                                        std::string(algorithm_name_) +
                                        " signature");
    }
    result.signature.resize(size);
    return result;
}

bool MlDsa::verify(const Bytes& message,
                   const Bytes& signature,
                   const Bytes& public_key) const {
    if (signature.size() != signature_bytes_) {
        return false;
    }

    detail::EvpPkey key = import_raw_key(public_key, true, algorithm_name_,
                                         public_key_bytes_);
    detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context) {
        detail::throw_openssl_error(CryptoErrorCode::verification_failed,
                                    std::string("failed to allocate ") +
                                        std::string(algorithm_name_) +
                                        " verification context");
    }

    if (EVP_DigestVerifyInit_ex(context.get(), nullptr, nullptr, nullptr,
                                nullptr, key.get(), nullptr) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::verification_failed,
                                    std::string("failed to initialize ") +
                                        std::string(algorithm_name_) +
                                        " verification");
    }

    const int result = EVP_DigestVerify(
        context.get(), signature.data(), signature.size(), message.data(),
        message.size());
    if (result == 1) {
        return true;
    }
    if (result == 0) {
        detail::clear_error_queue();
        return false;
    }
    detail::throw_openssl_error(CryptoErrorCode::verification_failed,
                                std::string(algorithm_name_) +
                                    " verification failed internally");
}

}  // namespace tcc::crypto::openssl
