#pragma once

#include <cstdint>

#include "tcc/benchmark/measurement.hpp"
#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::benchmark {

inline constexpr std::uint32_t warmup_iterations_per_run = 20;

struct RunConfig {
    std::uint32_t experiment_id;
    std::uint32_t run_id;
    protocol::AlgorithmId algorithm;
    std::uint32_t payload_bytes;
    ParticipantSide side;
    std::uint32_t warmup_iterations = warmup_iterations_per_run;
    std::uint32_t measured_iterations =
        protocol::limits::iterations_per_run;
};

void validate_run_config(const RunConfig& config);

}  // namespace tcc::benchmark
