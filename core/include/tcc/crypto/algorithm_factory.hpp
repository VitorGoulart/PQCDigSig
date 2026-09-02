#pragma once

#include <memory>

#include "tcc/crypto/signature_algorithm.hpp"
#include "tcc/protocol/algorithm_id.hpp"

namespace tcc::crypto {

[[nodiscard]] bool is_signature_algorithm_implemented(
    protocol::AlgorithmId algorithm_id) noexcept;

[[nodiscard]] std::unique_ptr<SignatureAlgorithm> create_signature_algorithm(
    protocol::AlgorithmId algorithm_id);

}  // namespace tcc::crypto
