#pragma once

#include <cstdint>

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

}  // namespace tcc::protocol
