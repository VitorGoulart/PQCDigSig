#pragma once

#include "tcc/crypto/bytes.hpp"

namespace tcc {

struct KeyPair {
    Bytes public_key;
    Bytes private_key;
};

}  // namespace tcc
