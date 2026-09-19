#include "crypto/adapters.hpp"

#include "crypto/openssl_helpers.hpp"
#include "tcc/crypto/error.hpp"

#include <climits>
#include <cstddef>
#include <memory>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

namespace tcc {
namespace {

constexpr char rsa_key_name[] = "RSA";
constexpr char rsa_digest_name[] = "SHA256";
constexpr int rsa_key_bits = 3072;
constexpr unsigned long rsa_public_exponent = 65537UL;
constexpr int rsa_salt_bytes = 32;
constexpr std::size_t rsa_public_key_bytes = 422U;
constexpr std::size_t rsa_signature_bytes = 384U;

struct BignumDeleter {
    void operator()(BIGNUM* value) const noexcept {
        BN_free(value);
    }
};

struct Pkcs8Deleter {
    void operator()(PKCS8_PRIV_KEY_INFO* value) const noexcept {
        PKCS8_PRIV_KEY_INFO_free(value);
    }
};

using Bignum = std::unique_ptr<BIGNUM, BignumDeleter>;
using Pkcs8 = std::unique_ptr<PKCS8_PRIV_KEY_INFO, Pkcs8Deleter>;

void validate_rsa_key(EVP_PKEY* key) {
    if (EVP_PKEY_is_a(key, rsa_key_name) != 1 ||
        EVP_PKEY_get_bits(key) != rsa_key_bits) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            "RSA key does not use the required 3072-bit parameters");
    }

    BIGNUM* raw_exponent = nullptr;
    if (EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_RSA_E, &raw_exponent) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            "failed to read RSA public exponent");
    }
    Bignum exponent{raw_exponent};
    if (BN_get_word(exponent.get()) != rsa_public_exponent) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            "RSA key does not use public exponent 65537");
    }
}

Bytes encode_rsa_public_key(EVP_PKEY* key) {
    const int encoded_size = i2d_PUBKEY(key, nullptr);
    if (encoded_size <= 0) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to query RSA public key encoding size");
    }

    Bytes encoded(static_cast<std::size_t>(encoded_size));
    unsigned char* cursor = encoded.data();
    if (i2d_PUBKEY(key, &cursor) != encoded_size) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to encode RSA public key");
    }
    return encoded;
}

Bytes encode_rsa_private_key(EVP_PKEY* key) {
    Pkcs8 private_key{EVP_PKEY2PKCS8(key)};
    if (!private_key) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to create RSA PKCS#8 private key");
    }

    const int encoded_size = i2d_PKCS8_PRIV_KEY_INFO(private_key.get(), nullptr);
    if (encoded_size <= 0) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to query RSA private key encoding size");
    }

    Bytes encoded(static_cast<std::size_t>(encoded_size));
    unsigned char* cursor = encoded.data();
    if (i2d_PKCS8_PRIV_KEY_INFO(private_key.get(), &cursor) != encoded_size) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to encode RSA private key");
    }
    return encoded;
}

long checked_der_size(const Bytes& encoded_key) {
    if (encoded_key.empty() ||
        encoded_key.size() > static_cast<std::size_t>(LONG_MAX)) {
        throw CryptoError(
            CryptoErrorCode::invalid_key,
            "invalid RSA DER key length");
    }
    return static_cast<long>(encoded_key.size());
}

openssl_detail::EvpPkey decode_rsa_public_key(const Bytes& encoded_key) {
    openssl_detail::clear_error_queue();
    const long encoded_size = checked_der_size(encoded_key);
    const unsigned char* cursor = encoded_key.data();
    const unsigned char* const end = cursor + encoded_key.size();
    EVP_PKEY* raw_key =
        d2i_PUBKEY_ex(nullptr, &cursor, encoded_size, nullptr, nullptr);
    if (raw_key == nullptr || cursor != end) {
        EVP_PKEY_free(raw_key);
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            "failed to decode RSA public key");
    }
    openssl_detail::EvpPkey key{raw_key};
    validate_rsa_key(key.get());
    return key;
}

openssl_detail::EvpPkey decode_rsa_private_key(const Bytes& encoded_key) {
    openssl_detail::clear_error_queue();
    const long encoded_size = checked_der_size(encoded_key);
    const unsigned char* cursor = encoded_key.data();
    const unsigned char* const end = cursor + encoded_key.size();
    Pkcs8 private_key{
        d2i_PKCS8_PRIV_KEY_INFO(nullptr, &cursor, encoded_size)};
    if (!private_key || cursor != end) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            "failed to decode RSA private key");
    }

    openssl_detail::EvpPkey key{
        EVP_PKCS82PKEY_ex(private_key.get(), nullptr, nullptr)};
    if (!key) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::invalid_key,
            "failed to import RSA private key");
    }
    validate_rsa_key(key.get());
    return key;
}

void initialize_rsa_pss(
    EVP_MD_CTX* context,
    EVP_PKEY* key,
    const bool signing) {
    char padding_mode[] = OSSL_PKEY_RSA_PAD_MODE_PSS;
    char mgf1_digest[] = "SHA256";
    int salt_length = rsa_salt_bytes;
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_utf8_string(
            OSSL_SIGNATURE_PARAM_PAD_MODE, padding_mode, 0),
        OSSL_PARAM_construct_utf8_string(
            OSSL_SIGNATURE_PARAM_MGF1_DIGEST, mgf1_digest, 0),
        OSSL_PARAM_construct_int(
            OSSL_SIGNATURE_PARAM_PSS_SALTLEN, &salt_length),
        OSSL_PARAM_construct_end(),
    };

    const int result =
        signing
            ? EVP_DigestSignInit_ex(
                  context, nullptr, rsa_digest_name, nullptr, nullptr, key,
                  parameters)
            : EVP_DigestVerifyInit_ex(
                  context, nullptr, rsa_digest_name, nullptr, nullptr, key,
                  parameters);
    if (result != 1) {
        openssl_detail::throw_openssl_error(
            signing ? CryptoErrorCode::signing_failed
                    : CryptoErrorCode::verification_failed,
            signing ? "failed to initialize RSASSA-PSS signing"
                    : "failed to initialize RSASSA-PSS verification");
    }
}

}  // namespace

Algorithm RsaPss::id() const noexcept {
    return Algorithm::rsa_pss;
}

std::size_t RsaPss::public_key_size() const noexcept {
    return rsa_public_key_bytes;
}

std::size_t RsaPss::maximum_signature_size() const noexcept {
    return rsa_signature_bytes;
}

KeyPair RsaPss::generate_key_pair() const {
    openssl_detail::clear_error_queue();
    openssl_detail::EvpPkeyContext context{
        EVP_PKEY_CTX_new_from_name(nullptr, rsa_key_name, nullptr)};
    Bignum exponent{BN_new()};
    if (!context || !exponent ||
        BN_set_word(exponent.get(), rsa_public_exponent) != 1 ||
        EVP_PKEY_keygen_init(context.get()) != 1 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), rsa_key_bits) != 1 ||
        EVP_PKEY_CTX_set1_rsa_keygen_pubexp(context.get(), exponent.get()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::provider_unavailable,
            "failed to initialize RSA-3072 key generation");
    }

    EVP_PKEY* raw_key = nullptr;
    if (EVP_PKEY_generate(context.get(), &raw_key) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::key_generation_failed,
            "failed to generate RSA-3072 key pair");
    }
    openssl_detail::EvpPkey key{raw_key};
    validate_rsa_key(key.get());
    KeyPair result{
        encode_rsa_public_key(key.get()),
        encode_rsa_private_key(key.get()),
    };
    if (result.public_key.size() != rsa_public_key_bytes) {
        throw CryptoError(
            CryptoErrorCode::key_generation_failed,
            "RSA-3072 returned an unexpected public key length");
    }
    return result;
}

Bytes RsaPss::sign(const Bytes& payload, const Bytes& private_key) const {
    openssl_detail::EvpPkey key = decode_rsa_private_key(private_key);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to allocate RSASSA-PSS signing context");
    }
    initialize_rsa_pss(context.get(), key.get(), true);

    if (EVP_DigestSignUpdate(context.get(), payload.data(), payload.size()) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to hash RSASSA-PSS payload");
    }

    std::size_t size = 0;
    if (EVP_DigestSignFinal(context.get(), nullptr, &size) != 1 || size == 0U) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to query RSASSA-PSS signature size");
    }

    Bytes signature(size);
    if (EVP_DigestSignFinal(context.get(), signature.data(), &size) != 1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::signing_failed,
            "failed to create RSASSA-PSS signature");
    }
    signature.resize(size);
    return signature;
}

bool RsaPss::verify(
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key) const {
    if (signature.size() != rsa_signature_bytes) {
        return false;
    }

    openssl_detail::EvpPkey key = decode_rsa_public_key(public_key);
    openssl_detail::EvpMessageDigestContext context{EVP_MD_CTX_new()};
    if (!context) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::verification_failed,
            "failed to allocate RSASSA-PSS verification context");
    }
    initialize_rsa_pss(context.get(), key.get(), false);

    if (EVP_DigestVerifyUpdate(context.get(), payload.data(), payload.size()) !=
        1) {
        openssl_detail::throw_openssl_error(
            CryptoErrorCode::verification_failed,
            "failed to hash RSASSA-PSS payload");
    }

    const int result = EVP_DigestVerifyFinal(
        context.get(), signature.data(), signature.size());
    if (result == 1) {
        return true;
    }
    if (result == 0) {
        openssl_detail::clear_error_queue();
        return false;
    }
    openssl_detail::throw_openssl_error(
        CryptoErrorCode::verification_failed,
        "RSASSA-PSS verification failed internally");
}

}  // namespace tcc
