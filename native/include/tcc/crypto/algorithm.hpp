#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace tcc {

enum class Algorithm : std::uint8_t {
    ml_dsa_44,
    ml_dsa_65,
    ml_dsa_87,
    falcon_512,
    falcon_1024,
    rsa_pss,
    ed25519,
};

enum class CryptoProvider : std::uint8_t {
    openssl,
    liboqs,
};

inline constexpr std::array<Algorithm, 7> algorithm_test_order{
    Algorithm::ml_dsa_44,
    Algorithm::ml_dsa_65,
    Algorithm::ml_dsa_87,
    Algorithm::falcon_512,
    Algorithm::falcon_1024,
    Algorithm::rsa_pss,
    Algorithm::ed25519,
};

[[nodiscard]] std::string_view algorithm_name(Algorithm algorithm) noexcept;
[[nodiscard]] CryptoProvider algorithm_provider(Algorithm algorithm);
[[nodiscard]] std::string_view provider_name(CryptoProvider provider) noexcept;

}  // namespace tcc
