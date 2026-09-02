#pragma once

#include <cstdint>
#include <vector>

namespace tcc::crypto {

using Bytes = std::vector<std::uint8_t>;

struct KeyPair {
    Bytes public_key;
    Bytes private_key;
};

}  // namespace tcc::crypto
