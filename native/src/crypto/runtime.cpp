#include "tcc/crypto/runtime.hpp"

#include "crypto/openssl_helpers.hpp"

#include <array>
#include <memory>
#include <string>

#include <openssl/crypto.h>
#include <openssl/provider.h>
#include <openssl/rand.h>
#include <oqs/oqs.h>

namespace tcc {
namespace {

struct ProviderDeleter {
    void operator()(OSSL_PROVIDER* value) const noexcept {
        OSSL_PROVIDER_unload(value);
    }
};

struct KeyManagementDeleter {
    void operator()(EVP_KEYMGMT* value) const noexcept {
        EVP_KEYMGMT_free(value);
    }
};

struct SignatureDeleter {
    void operator()(EVP_SIGNATURE* value) const noexcept {
        EVP_SIGNATURE_free(value);
    }
};

struct DigestDeleter {
    void operator()(EVP_MD* value) const noexcept {
        EVP_MD_free(value);
    }
};

constexpr std::array<const char*, 5> openssl_algorithms{
    "ML-DSA-44", "ML-DSA-65", "ML-DSA-87", "RSA", "ED25519",
};
constexpr std::array<const char*, 4> openssl_digests{
    "SHA2-256", "SHA2-512", "SHAKE-128", "SHAKE-256",
};

class CryptoRuntime final {
public:
    CryptoRuntime() {
        if (OPENSSL_init_crypto(
                OPENSSL_INIT_LOAD_CRYPTO_STRINGS | OPENSSL_INIT_LOAD_CONFIG,
                nullptr) != 1) {
            openssl_detail::throw_openssl_error(
                CryptoErrorCode::provider_unavailable,
                "failed to initialize OpenSSL");
        }
        openssl_detail::clear_error_queue();
        provider_.reset(OSSL_PROVIDER_load(nullptr, "default"));
        if (!provider_) {
            openssl_detail::throw_openssl_error(
                CryptoErrorCode::provider_unavailable,
                "failed to load the default OpenSSL provider");
        }

        for (std::size_t i = 0; i < openssl_algorithms.size(); ++i) {
            const auto name = openssl_algorithms[i];
            key_management_[i].reset(EVP_KEYMGMT_fetch(nullptr, name, nullptr));
            signatures_[i].reset(EVP_SIGNATURE_fetch(nullptr, name, nullptr));
            if (!key_management_[i] || !signatures_[i]) {
                openssl_detail::throw_openssl_error(
                    CryptoErrorCode::provider_unavailable,
                    std::string("failed to prepare OpenSSL algorithm ") + name);
            }
        }
        for (std::size_t i = 0; i < openssl_digests.size(); ++i) {
            digests_[i].reset(EVP_MD_fetch(nullptr, openssl_digests[i], nullptr));
            if (!digests_[i]) {
                openssl_detail::throw_openssl_error(
                    CryptoErrorCode::provider_unavailable,
                    std::string("failed to prepare OpenSSL digest ") +
                        openssl_digests[i]);
            }
        }

        OQS_init();
    }

private:
    // Keep the provider and fetched implementations alive until shutdown.
    std::unique_ptr<OSSL_PROVIDER, ProviderDeleter> provider_;
    std::array<std::unique_ptr<EVP_KEYMGMT, KeyManagementDeleter>, 5> key_management_;
    std::array<std::unique_ptr<EVP_SIGNATURE, SignatureDeleter>, 5> signatures_;
    std::array<std::unique_ptr<EVP_MD, DigestDeleter>, 4> digests_;
};

}  // namespace

void initialize_crypto_runtime() {
    // Local static initialization also serializes concurrent first callers.
    static const CryptoRuntime runtime;
    static_cast<void>(runtime);

    thread_local bool random_generators_ready = false;
    if (!random_generators_ready) {
        openssl_detail::clear_error_queue();
        // In OpenSSL 3.5 these getters create and instantiate the generators.
        // Their initial seeding is prepared here; operation randomness remains
        // part of the measured operation, using the default secure generators.
        if (RAND_get0_primary(nullptr) == nullptr ||
            RAND_get0_public(nullptr) == nullptr ||
            RAND_get0_private(nullptr) == nullptr) {
            openssl_detail::throw_openssl_error(
                CryptoErrorCode::provider_unavailable,
                "failed to prepare OpenSSL random generators");
        }
        random_generators_ready = true;
    }
}

}  // namespace tcc
