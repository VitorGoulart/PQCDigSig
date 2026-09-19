#pragma once

#include "tcc/crypto/signature_algorithm.hpp"

#include <memory>

namespace tcc {

[[nodiscard]] bool is_supported_algorithm(Algorithm algorithm) noexcept;

[[nodiscard]] std::unique_ptr<SignatureAlgorithm> create_signature_algorithm(
    Algorithm algorithm);

}  // namespace tcc
