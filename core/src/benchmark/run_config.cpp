#include "tcc/benchmark/run_config.hpp"

#include <stdexcept>

namespace tcc::benchmark {
namespace {

[[nodiscard]] bool is_supported_algorithm(
    const protocol::AlgorithmId algorithm) noexcept {
    switch (algorithm) {
        case protocol::AlgorithmId::ml_dsa_44:
        case protocol::AlgorithmId::ml_dsa_65:
        case protocol::AlgorithmId::ml_dsa_87:
        case protocol::AlgorithmId::falcon_512:
        case protocol::AlgorithmId::falcon_1024:
        case protocol::AlgorithmId::rsa_pss:
        case protocol::AlgorithmId::ed25519:
            return true;
        case protocol::AlgorithmId::invalid:
            return false;
    }
    return false;
}

[[nodiscard]] bool is_supported_payload(
    const std::uint32_t payload_bytes) noexcept {
    return payload_bytes == protocol::limits::payload_10_kib ||
           payload_bytes == protocol::limits::payload_100_kib ||
           payload_bytes == protocol::limits::payload_1_mib;
}

[[nodiscard]] bool is_supported_side(const ParticipantSide side) noexcept {
    return side == ParticipantSide::client || side == ParticipantSide::server;
}

}  // namespace

void validate_run_config(const RunConfig& config) {
    if (config.experiment_id == 0 || config.run_id == 0) {
        throw std::invalid_argument("experiment_id and run_id must be positive");
    }
    if (!is_supported_algorithm(config.algorithm)) {
        throw std::invalid_argument("unsupported signature algorithm");
    }
    if (!is_supported_payload(config.payload_bytes)) {
        throw std::invalid_argument("unsupported payload size");
    }
    if (!is_supported_side(config.side)) {
        throw std::invalid_argument("invalid participant side");
    }
    if (config.warmup_iterations != warmup_iterations_per_run) {
        throw std::invalid_argument("a run must have exactly 20 warmup iterations");
    }
    if (config.measured_iterations != protocol::limits::iterations_per_run) {
        throw std::invalid_argument("a run must have exactly 1000 measured iterations");
    }
}

}  // namespace tcc::benchmark
