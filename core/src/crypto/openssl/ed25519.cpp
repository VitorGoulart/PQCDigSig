#include "crypto/openssl/ed25519.hpp"

#include <cstddef>

#include "crypto/openssl/openssl_helpers.hpp"
#include "tcc/crypto/crypto_error.hpp"

namespace tcc::crypto::openssl {
namespace {

constexpr char algorithm_name[] = "ED25519";
constexpr std::size_t key_bytes = 32;
constexpr std::size_t signature_bytes = 64;

Bytes export_raw_key(EVP_PKEY* key,
                     const bool public_component,
                     const CryptoErrorCode error_code) {
    std::size_t size = 0;
    const int query_result = public_component
                                 ? EVP_PKEY_get_raw_public_key(key, nullptr, &size)
                                 : EVP_PKEY_get_raw_private_key(key, nullptr, &size);
    if (query_result != 1 || size != key_bytes) {
        detail::throw_openssl_error(error_code,
                                    "failed to query Ed25519 key size");
    }

    Bytes output(size);
    const int export_result =
        public_component
            ? EVP_PKEY_get_raw_public_key(key, output.data(), &size)
            : EVP_PKEY_get_raw_private_key(key, output.data(), &size);
    if (export_result != 1 || size != key_bytes) {
        detail::throw_openssl_error(error_code, "failed to export Ed25519 key");
    }
    return output;
}

detail::EvpPkey import_raw_key(const Bytes& encoded_key,
                              const bool public_component) {
    if (encoded_key.size() != key_bytes) {
        throw CryptoError(CryptoErrorCode::invalid_key,
                          "invalid Ed25519 key length");
    }

    detail::clear_error_queue();
    EVP_PKEY* raw_key =
        public_component
            ? EVP_PKEY_new_raw_public_key_ex(nullptr, algorithm_name, nullptr,
                                             encoded_key.data(), encoded_key.size())
            : EVP_PKEY_new_raw_private_key_ex(nullptr, algorithm_name, nullptr,
                                              encoded_key.data(), encoded_key.size());
    if (raw_key == nullptr) {
        detail::throw_openssl_error(CryptoErrorCode::invalid_key,
                                    "failed to import Ed25519 key");
    }
    return detail::EvpPkey{raw_key};
}

}  // namespace

protocol::AlgorithmId Ed25519::id() const noexcept {
    return protocol::AlgorithmId::ed25519;
}

std::string_view Ed25519::name() const noexcept {
    return "Ed25519";
}

std::string_view Ed25519::provider() const noexcept {
    return "OpenSSL";
}

KeyPair Ed25519::generate_key_pair() const {
    detail::clear_error_queue();
    detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, algorithm_name, nullptr)};
    if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::provider_unavailable,
                                    "failed to initialize Ed25519 key generation");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_generate(context.get(), &raw_key) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::key_generation_failed,
                                    "failed to generate Ed25519 key pair");
    }
    detail::EvpPkey key{raw_key};

    KeyPair result;
    result.public_key =
        export_raw_key(key.get(), true, CryptoErrorCode::key_generation_failed);
    result.private_key =
        export_raw_key(key.get(), false, CryptoErrorCode::key_generation_failed);
    return result;
}

SignatureResult Ed25519::sign(const Bytes& message,
                              const Bytes& private_key) const {
    detail::EvpPkey key = import_raw_key(private_key, false);
    detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestSignInit_ex(context.get(), nullptr, nullptr, nullptr, nullptr,
                              key.get(), nullptr) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    "failed to initialize Ed25519 signing");
    }

    SignatureResult result{Bytes(signature_bytes)};
    std::size_t size = result.signature.size();
    if (EVP_DigestSign(context.get(), result.signature.data(), &size,
                       message.data(), message.size()) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::signing_failed,
                                    "failed to create Ed25519 signature");
    }
    result.signature.resize(size);
    return result;
}

bool Ed25519::verify(const Bytes& message,
                     const Bytes& signature,
                     const Bytes& public_key) const {
    if (signature.size() != signature_bytes) {
        return false;
    }

    detail::EvpPkey key = import_raw_key(public_key, true);
    detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context ||
        EVP_DigestVerifyInit_ex(context.get(), nullptr, nullptr, nullptr,
                                nullptr, key.get(), nullptr) != 1) {
        detail::throw_openssl_error(CryptoErrorCode::verification_failed,
                                    "failed to initialize Ed25519 verification");
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
                                "Ed25519 verification failed internally");
}

}  // namespace tcc::crypto::openssl
