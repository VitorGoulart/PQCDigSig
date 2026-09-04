#pragma once

#include <cstdint>
#include <string_view>

#include "tcc/protocol/algorithm_id.hpp"

namespace tcc::benchmark {

enum class ParticipantSide : std::uint8_t {
    client,
    server,
};

enum class Operation : std::uint8_t {
    key_generation,
    signing,
    verification,
};

struct Measurement {
    std::uint32_t experiment_id;
    std::uint32_t run_id;
    std::uint32_t iteration;
    ParticipantSide side;
    protocol::AlgorithmId algorithm;
    std::uint32_t payload_bytes;
    Operation operation;
    std::int64_t duration_ns;
};

[[nodiscard]] constexpr std::string_view to_string(
    const ParticipantSide side) noexcept {
    switch (side) {
        case ParticipantSide::client:
            return "client";
        case ParticipantSide::server:
            return "server";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view to_string(
    const Operation operation) noexcept {
    switch (operation) {
        case Operation::key_generation:
            return "keygen";
        case Operation::signing:
            return "sign";
        case Operation::verification:
            return "verify";
    }
    return "unknown";
}

}  // namespace tcc::benchmark
