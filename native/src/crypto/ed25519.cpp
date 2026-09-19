#include "crypto/adapters.hpp"

#include "crypto/openssl_helpers.hpp"
#include "tcc/crypto/error.hpp"

#include <cstddef>

namespace tcc {
namespace {

constexpr char ed25519_openssl_name[] = "ED25519";
constexpr std::size_t ed25519_key_bytes = 32U;
constexpr std::size_t ed25519_signature_bytes = 64U;

Bytes export_ed25519_key(EVP_PKEY* key, const bool public_component) {
    std::size_t size = 0;
    const int query_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, nullptr, &size)
            : EVP_PKEY_get_raw_private_key(key, nullptr, &size);
    if (query_result != 1 || size != ed25519_key_bytes) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to query Ed25519 key size");
    }

    Bytes output(size);
    const int export_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, output.data(), &size)
            : EVP_PKEY_get_raw_private_key(key, output.data(), &size);
    if (export_result != 1 || size != ed25519_key_bytes) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to export Ed25519 key");
    }
    return output;
}

openssl_detail::EvpPkey import_ed25519_key(
    const Bytes& encoded_key,
    const bool public_component) {
    if (encoded_key.size() != ed25519_key_bytes) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            "invalid Ed25519 key length");
    }

    openssl_detail::clear_error_queue();
    EVP_PKEY* raw_key =
        public_component
            ? EVP_PKEY_new_raw_public_key_ex(
                  nullptr, ed25519_openssl_name, nullptr, encoded_key.data(),
                  encoded_key.size())
            : EVP_PKEY_new_raw_private_key_ex(
                  nullptr, ed25519_openssl_name, nullptr, encoded_key.data(),
                  encoded_key.size());
    if (raw_key == nullptr) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            "failed to import Ed25519 key");
    }
    return openssl_detail::EvpPkey{raw_key};
}

}  // namespace

Algorithm Ed25519::id() const noexcept {
    return Algorithm::ed25519;
}

std::size_t Ed25519::public_key_size() const noexcept {
    return ed25519_key_bytes;
}

std::size_t Ed25519::maximum_signature_size() const noexcept {
    return ed25519_signature_bytes;
}

KeyPair Ed25519::generate_key_pair() const {
    openssl_detail::clear_error_queue();
    openssl_detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, ed25519_openssl_name, nullptr)};
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::provider_unavailable,
            "failed to initialize Ed25519 key generation");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_generate(context.get(), &raw_key) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to generate Ed25519 key pair");
    }
    openssl_detail::EvpPkey key{raw_key};
    return {
        export_ed25519_key(key.get(), true),
        export_ed25519_key(key.get(), false),
    };
}

Bytes Ed25519::sign(const Bytes& payload, const Bytes& private_key) const {
    openssl_detail::EvpPkey key = import_ed25519_key(private_key, false);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestSignInit_ex(
            context.get(), nullptr, nullptr, nullptr, nullptr, key.get(),
            nullptr) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to initialize Ed25519 signing");
    }

    Bytes signature(ed25519_signature_bytes);
    std::size_t size = signature.size();
    if (EVP_DigestSign(
            context.get(), signature.data(), &size, payload.data(),
            payload.size()) != 1 ||
        size != ed25519_signature_bytes) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to create Ed25519 signature");
    }
    signature.resize(size);
    return signature;
}

bool Ed25519::verify(
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key) const {
    if (signature.size() != ed25519_signature_bytes) {
        return false;
    }

    openssl_detail::EvpPkey key = import_ed25519_key(public_key, true);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestVerifyInit_ex(
            context.get(), nullptr, nullptr, nullptr, nullptr, key.get(),
            nullptr) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::verification_failed,
            "failed to initialize Ed25519 verification");
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
        "Ed25519 verification failed internally");
}

}  // namespace tcc
