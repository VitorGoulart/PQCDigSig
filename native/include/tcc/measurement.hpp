#pragma once

#include "tcc/crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tcc {

enum class Operation { keygen, sign, verify };
enum class Side { client, server };

[[nodiscard]] std::string_view operation_name(Operation operation);
[[nodiscard]] std::string_view side_name(Side side);

struct Measurement {
    std::uint32_t repetition{};
    std::string client;
    Side side{};
    Algorithm algorithm{};
    std::size_t payload_bytes{};
    Operation operation{};
    std::int64_t duration_ns{};
    bool success{};
};

template <typename Value>
struct TimedResult {
    Value value;
    std::int64_t duration_ns{};
};

[[nodiscard]] TimedResult<KeyPair> measure_key_generation(
    const SignatureAlgorithm& algorithm);
[[nodiscard]] TimedResult<Bytes> measure_signature(
    const SignatureAlgorithm& algorithm,
    const Bytes& payload,
    const Bytes& private_key);
[[nodiscard]] TimedResult<bool> measure_verification(
    const SignatureAlgorithm& algorithm,
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key);

}  // namespace tcc
