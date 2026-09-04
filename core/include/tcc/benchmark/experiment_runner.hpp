#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "tcc/benchmark/run_config.hpp"
#include "tcc/benchmark/run_context.hpp"
#include "tcc/crypto/signature_algorithm.hpp"

namespace tcc::benchmark {

class ExperimentRunner final {
  public:
    explicit ExperimentRunner(RunConfig config);
    ExperimentRunner(RunConfig config,
                     std::unique_ptr<crypto::SignatureAlgorithm> algorithm);

    ExperimentRunner(const ExperimentRunner&) = delete;
    ExperimentRunner& operator=(const ExperimentRunner&) = delete;
    ExperimentRunner(ExperimentRunner&&) noexcept = default;
    ExperimentRunner& operator=(ExperimentRunner&&) noexcept = default;

    [[nodiscard]] const RunConfig& config() const noexcept;
    [[nodiscard]] const RunContext& context() const noexcept;

    void warm_up(const crypto::Bytes& signed_content);
    void begin_iteration(std::uint32_t iteration);
    [[nodiscard]] SignedArtifact sign_outbound(
        const crypto::Bytes& signed_content);
    [[nodiscard]] bool verify_inbound(const crypto::Bytes& signed_content,
                                      const crypto::Bytes& signature,
                                      const crypto::Bytes& public_key);
    [[nodiscard]] IterationMeasurements finish_iteration();
    void abort() noexcept;

  private:
    struct PendingMeasurements {
        std::optional<std::int64_t> key_generation_ns;
        std::optional<std::int64_t> signing_ns;
        std::optional<std::int64_t> verification_ns;
    };

    void require_active_iteration() const;
    [[nodiscard]] Measurement make_measurement(Operation operation,
                                               std::int64_t duration_ns) const;

    RunConfig config_;
    std::unique_ptr<crypto::SignatureAlgorithm> algorithm_;
    RunContext context_;
    PendingMeasurements pending_;
};

}  // namespace tcc::benchmark
