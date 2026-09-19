#include "crypto/adapters.hpp"

#include "crypto/openssl_helpers.hpp"
#include "tcc/crypto/error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

#include <openssl/core_names.h>
#include <openssl/params.h>

namespace tcc {
namespace {

struct MlDsaParameters {
    std::string_view openssl_name;
    std::size_t public_key_bytes;
    std::size_t private_key_bytes;
    std::size_t signature_bytes;
};

MlDsaParameters ml_dsa_parameters(const Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::ml_dsa_44:
            return {"ML-DSA-44", 1312U, 2560U, 2420U};
        case Algorithm::ml_dsa_65:
            return {"ML-DSA-65", 1952U, 4032U, 3309U};
        case Algorithm::ml_dsa_87:
            return {"ML-DSA-87", 2592U, 4896U, 4627U};
        default:
            throw CryptoError(
                CryptoErrorCode::unsupported_algorithm,
                "OpenSSL ML-DSA adapter does not support the requested algorithm");
    }
}

Bytes export_ml_dsa_key(
    EVP_PKEY* key,
    const bool public_component,
    const std::string_view openssl_name) {
    std::size_t size = 0;
    const int query_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, nullptr, &size)
            : EVP_PKEY_get_raw_private_key(key, nullptr, &size);
    if (query_result != 1 || size == 0U) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            std::string("failed to query ") + std::string(openssl_name) +
                " key size");
    }

    Bytes output(size);
    const int export_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, output.data(), &size)
            : EVP_PKEY_get_raw_private_key(key, output.data(), &size);
    if (export_result != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            std::string("failed to export ") + std::string(openssl_name) +
                " key");
    }
    output.resize(size);
    return output;
}

openssl_detail::EvpPkey import_ml_dsa_key(
    const Bytes& encoded_key,
    const bool public_component,
    const std::string_view openssl_name,
    const std::size_t expected_size) {
    if (encoded_key.size() != expected_size) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            std::string("invalid ") + std::string(openssl_name) +
                " key length");
    }

    openssl_detail::clear_error_queue();
    openssl_detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, openssl_name.data(), nullptr)};
    if (!context || EVP_PKEY_fromdata_init(context.get()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::provider_unavailable,
            std::string("failed to initialize ") + std::string(openssl_name) +
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
    const int selection =
        public_component ? EVP_PKEY_PUBLIC_KEY : EVP_PKEY_PRIVATE_KEY;
    if (EVP_PKEY_fromdata(context.get(), &raw_key, selection, parameters) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            std::string("failed to import ") + std::string(openssl_name) +
                " key");
    }
    return openssl_detail::EvpPkey{raw_key};
}

}  // namespace

MlDsa::MlDsa(const Algorithm algorithm)
    : algorithm_(algorithm),
      openssl_name_(),
      public_key_bytes_(0U),
      private_key_bytes_(0U),
      signature_bytes_(0U) {
    const MlDsaParameters parameters = ml_dsa_parameters(algorithm);
    openssl_name_ = parameters.openssl_name;
    public_key_bytes_ = parameters.public_key_bytes;
    private_key_bytes_ = parameters.private_key_bytes;
    signature_bytes_ = parameters.signature_bytes;
}

Algorithm MlDsa::id() const noexcept {
    return algorithm_;
}

std::size_t MlDsa::public_key_size() const noexcept {
    return public_key_bytes_;
}

std::size_t MlDsa::maximum_signature_size() const noexcept {
    return signature_bytes_;
}

KeyPair MlDsa::generate_key_pair() const {
    openssl_detail::clear_error_queue();
    openssl_detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, openssl_name_.data(), nullptr)};
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::provider_unavailable,
            std::string("failed to initialize ") + std::string(openssl_name_) +
                " key generation");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_generate(context.get(), &raw_key) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            std::string("failed to generate ") + std::string(openssl_name_) +
                " key pair");
    }
    openssl_detail::EvpPkey key{raw_key};

    KeyPair result{
        export_ml_dsa_key(key.get(), true, openssl_name_),
        export_ml_dsa_key(key.get(), false, openssl_name_),
    };
    if (result.public_key.size() != public_key_bytes_ ||
        result.private_key.size() != private_key_bytes_) {
        throw CryptoError(
            CryptoErrorCode::key_generation_failed,
            std::string(openssl_name_) + " returned unexpected key lengths");
    }
    return result;
}

Bytes MlDsa::sign(const Bytes& payload, const Bytes& private_key) const {
    openssl_detail::EvpPkey key = import_ml_dsa_key(
        private_key, false, openssl_name_, private_key_bytes_);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestSignInit_ex(
            context.get(), nullptr, nullptr, nullptr, nullptr, key.get(),
            nullptr) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            std::string("failed to initialize ") + std::string(openssl_name_) +
                " signing");
    }

    std::size_t size = 0;
    if (EVP_DigestSign(
            context.get(), nullptr, &size, payload.data(), payload.size()) != 1 ||
        size == 0U) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            std::string("failed to query ") + std::string(openssl_name_) +
                " signature size");
    }

    Bytes signature(size);
    if (EVP_DigestSign(
            context.get(), signature.data(), &size, payload.data(),
            payload.size()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            std::string("failed to create ") + std::string(openssl_name_) +
                " signature");
    }
    signature.resize(size);
    return signature;
}

bool MlDsa::verify(
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key) const {
    if (signature.size() != signature_bytes_) {
        return false;
    }

    openssl_detail::EvpPkey key = import_ml_dsa_key(
        public_key, true, openssl_name_, public_key_bytes_);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestVerifyInit_ex(
            context.get(), nullptr, nullptr, nullptr, nullptr, key.get(),
            nullptr) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::verification_failed,
            std::string("failed to initialize ") + std::string(openssl_name_) +
                " verification");
    }

    const int result = EVP_DigestVerify(
        context.get(), signature.data(), signature.size(), payload.data(),
        payload.size());
    if (result == 1) {
        return true;
    }
    if (result == 0) {
        openssl_detail::clear_error_queue();
        return false;
    }
    openssl_detail::throw_openssl_error(
        CryptoErrorCode::verification_failed,
        std::string(openssl_name_) + " verification failed internally");
}

}  // namespace tcc
