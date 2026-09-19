#include "tcc/crypto.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>
#include <utility>

namespace {

class TestAlgorithm final : public tcc::SignatureAlgorithm {
public:
    [[nodiscard]] tcc::Algorithm id() const noexcept override {
        return tcc::Algorithm::ed25519;
    }

    [[nodiscard]] std::size_t public_key_size() const noexcept override {
        return 1U;
    }

    [[nodiscard]] std::size_t maximum_signature_size() const noexcept override {
        return 2U;
    }

    [[nodiscard]] tcc::KeyPair generate_key_pair() const override {
        return {{0x01U}, {0x02U}};
    }

    [[nodiscard]] tcc::Bytes sign(
        const tcc::Bytes& payload,
        const tcc::Bytes& private_key) const override {
        return {static_cast<std::uint8_t>(payload.size()), private_key.at(0)};
    }

    [[nodiscard]] bool verify(
        const tcc::Bytes& payload,
        const tcc::Bytes& signature,
        const tcc::Bytes& public_key) const override {
        return !payload.empty() && !signature.empty() && !public_key.empty();
    }
};

}  // namespace

int main() {
    constexpr std::array<std::string_view, 7> expected_names{
        "ML-DSA-44",
        "ML-DSA-65",
        "ML-DSA-87",
        "Falcon-512",
        "Falcon-1024",
        "RSASSA-PSS",
        "Ed25519",
    };

    for (std::size_t index = 0; index < expected_names.size(); ++index) {
        assert(tcc::algorithm_name(tcc::algorithm_test_order[index]) ==
               expected_names[index]);
    }

    assert(tcc::algorithm_provider(tcc::Algorithm::ml_dsa_44) ==
           tcc::CryptoProvider::openssl);
    assert(tcc::algorithm_provider(tcc::Algorithm::ml_dsa_65) ==
           tcc::CryptoProvider::openssl);
    assert(tcc::algorithm_provider(tcc::Algorithm::ml_dsa_87) ==
           tcc::CryptoProvider::openssl);
    assert(tcc::algorithm_provider(tcc::Algorithm::rsa_pss) ==
           tcc::CryptoProvider::openssl);
    assert(tcc::algorithm_provider(tcc::Algorithm::ed25519) ==
           tcc::CryptoProvider::openssl);
    assert(tcc::algorithm_provider(tcc::Algorithm::falcon_512) ==
           tcc::CryptoProvider::liboqs);
    assert(tcc::algorithm_provider(tcc::Algorithm::falcon_1024) ==
           tcc::CryptoProvider::liboqs);
    assert(tcc::provider_name(tcc::CryptoProvider::openssl) == "OpenSSL");
    assert(tcc::provider_name(tcc::CryptoProvider::liboqs) == "liboqs");

    TestAlgorithm algorithm;
    const auto key_pair = algorithm.generate_key_pair();
    const tcc::Bytes payload{0xA5U};
    const auto signature = algorithm.sign(payload, key_pair.private_key);

    assert(algorithm.name() == "Ed25519");
    assert(algorithm.provider() == tcc::CryptoProvider::openssl);
    assert(algorithm.public_key_size() == key_pair.public_key.size());
    assert(algorithm.maximum_signature_size() == signature.size());
    assert(algorithm.verify(payload, signature, key_pair.public_key));

    try {
        static_cast<void>(
            tcc::algorithm_provider(static_cast<tcc::Algorithm>(0xFFU)));
        return 1;
    } catch (const tcc::CryptoError& error) {
        assert(error.code() == tcc::CryptoErrorCode::unsupported_algorithm);
        assert(std::string_view(error.what()) ==
               "unsupported signature algorithm");
    }

    return 0;
}
