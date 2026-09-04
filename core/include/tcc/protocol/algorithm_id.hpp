#pragma once

#include <cstdint>
#include <string_view>

namespace tcc::protocol {

enum class AlgorithmId : std::uint8_t {
    invalid = 0x00,
    ml_dsa_44 = 0x01,
    ml_dsa_65 = 0x02,
    ml_dsa_87 = 0x03,
    falcon_512 = 0x04,
    falcon_1024 = 0x05,
    rsa_pss = 0x06,
    ed25519 = 0x07,
};

[[nodiscard]] constexpr std::string_view algorithm_name(
    const AlgorithmId algorithm) noexcept {
    switch (algorithm) {
        case AlgorithmId::ml_dsa_44:
            return "ML-DSA-44";
        case AlgorithmId::ml_dsa_65:
            return "ML-DSA-65";
        case AlgorithmId::ml_dsa_87:
            return "ML-DSA-87";
        case AlgorithmId::falcon_512:
            return "Falcon-512";
        case AlgorithmId::falcon_1024:
            return "Falcon-1024";
        case AlgorithmId::rsa_pss:
            return "RSASSA-PSS";
        case AlgorithmId::ed25519:
            return "Ed25519";
        case AlgorithmId::invalid:
            return {};
    }
    return {};
}

}  // namespace tcc::protocol
