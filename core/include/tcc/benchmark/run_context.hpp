#pragma once

#include <array>
#include <cstdint>

#include "tcc/benchmark/measurement.hpp"
#include "tcc/crypto/key_pair.hpp"

namespace tcc::benchmark {

enum class RunState : std::uint8_t {
    awaiting_warmup,
    ready,
    active_iteration,
    completed,
    failed,
};

struct RunContext {
    RunState state = RunState::awaiting_warmup;
    std::uint32_t next_iteration = 0;
    std::uint32_t completed_iterations = 0;
};

struct SignedArtifact {
    crypto::Bytes public_key;
    crypto::Bytes signature;
};

struct IterationMeasurements {
    Measurement key_generation;
    Measurement signing;
    Measurement verification;

    [[nodiscard]] std::array<Measurement, 3> as_array() const {
        return {key_generation, signing, verification};
    }
};

}  // namespace tcc::benchmark
