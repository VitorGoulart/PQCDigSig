#include "tcc/benchmark/experiment_runner.hpp"

#include <stdexcept>
#include <utility>

#include "tcc/benchmark/timer.hpp"
#include "tcc/crypto/algorithm_factory.hpp"
#include "tcc/crypto/crypto_error.hpp"

namespace tcc::benchmark {

ExperimentRunner::ExperimentRunner(RunConfig config)
    : config_(config) {
    validate_run_config(config_);
    algorithm_ = crypto::create_signature_algorithm(config_.algorithm);
}

ExperimentRunner::ExperimentRunner(
    RunConfig config,
    std::unique_ptr<crypto::SignatureAlgorithm> algorithm)
    : config_(config), algorithm_(std::move(algorithm)) {
    validate_run_config(config_);
    if (!algorithm_) {
        throw std::invalid_argument("signature algorithm must not be null");
    }
    if (algorithm_->id() != config_.algorithm) {
        throw std::invalid_argument(
            "signature algorithm does not match the run configuration");
    }
}

const RunConfig& ExperimentRunner::config() const noexcept {
    return config_;
}

const RunContext& ExperimentRunner::context() const noexcept {
    return context_;
}

void ExperimentRunner::warm_up(const crypto::Bytes& signed_content) {
    if (context_.state != RunState::awaiting_warmup) {
        throw std::logic_error("run warmup is not allowed in the current state");
    }

    try {
        for (std::uint32_t iteration = 0;
             iteration < config_.warmup_iterations;
             ++iteration) {
            const crypto::KeyPair key_pair = algorithm_->generate_key_pair();
            const crypto::SignatureResult result =
                algorithm_->sign(signed_content, key_pair.private_key);
            if (!algorithm_->verify(signed_content,
                                    result.signature,
                                    key_pair.public_key)) {
                context_.state = RunState::failed;
                throw crypto::CryptoError(
                    crypto::CryptoErrorCode::verification_failed,
                    "warmup signature verification failed");
            }
        }
    } catch (...) {
        context_.state = RunState::failed;
        throw;
    }

    context_.state = RunState::ready;
}

void ExperimentRunner::begin_iteration(const std::uint32_t iteration) {
    if (context_.state != RunState::ready) {
        throw std::logic_error("cannot begin an iteration in the current state");
    }
    if (iteration != context_.next_iteration) {
        throw std::invalid_argument("iteration is not the next expected value");
    }
    pending_ = PendingMeasurements{};
    context_.state = RunState::active_iteration;
}

SignedArtifact ExperimentRunner::sign_outbound(
    const crypto::Bytes& signed_content) {
    require_active_iteration();
    if (pending_.key_generation_ns || pending_.signing_ns) {
        throw std::logic_error("outbound signature was already produced");
    }

    try {
        const Timer::TimePoint keygen_start = Timer::now();
        crypto::KeyPair key_pair = algorithm_->generate_key_pair();
        const Timer::TimePoint keygen_end = Timer::now();

        const Timer::TimePoint signing_start = Timer::now();
        crypto::SignatureResult result =
            algorithm_->sign(signed_content, key_pair.private_key);
        const Timer::TimePoint signing_end = Timer::now();

        pending_.key_generation_ns =
            Timer::elapsed_nanoseconds(keygen_start, keygen_end);
        pending_.signing_ns =
            Timer::elapsed_nanoseconds(signing_start, signing_end);
        return {std::move(key_pair.public_key), std::move(result.signature)};
    } catch (...) {
        context_.state = RunState::failed;
        throw;
    }
}

bool ExperimentRunner::verify_inbound(const crypto::Bytes& signed_content,
                                      const crypto::Bytes& signature,
                                      const crypto::Bytes& public_key) {
    require_active_iteration();
    if (pending_.verification_ns) {
        throw std::logic_error("inbound signature was already verified");
    }

    try {
        const Timer::TimePoint verification_start = Timer::now();
        const bool valid =
            algorithm_->verify(signed_content, signature, public_key);
        const Timer::TimePoint verification_end = Timer::now();

        if (!valid) {
            context_.state = RunState::failed;
            return false;
        }
        pending_.verification_ns =
            Timer::elapsed_nanoseconds(verification_start, verification_end);
        return true;
    } catch (...) {
        context_.state = RunState::failed;
        throw;
    }
}

IterationMeasurements ExperimentRunner::finish_iteration() {
    require_active_iteration();
    if (!pending_.key_generation_ns || !pending_.signing_ns ||
        !pending_.verification_ns) {
        throw std::logic_error(
            "iteration requires key generation, signing and verification");
    }

    IterationMeasurements measurements{
        make_measurement(Operation::key_generation,
                         *pending_.key_generation_ns),
        make_measurement(Operation::signing, *pending_.signing_ns),
        make_measurement(Operation::verification, *pending_.verification_ns),
    };

    ++context_.completed_iterations;
    ++context_.next_iteration;
    pending_ = PendingMeasurements{};
    context_.state =
        context_.completed_iterations == config_.measured_iterations
            ? RunState::completed
            : RunState::ready;
    return measurements;
}

void ExperimentRunner::abort() noexcept {
    if (context_.state != RunState::completed) {
        pending_ = PendingMeasurements{};
        context_.state = RunState::failed;
    }
}

void ExperimentRunner::require_active_iteration() const {
    if (context_.state != RunState::active_iteration) {
        throw std::logic_error("there is no active iteration");
    }
}

Measurement ExperimentRunner::make_measurement(
    const Operation operation,
    const std::int64_t duration_ns) const {
    return {config_.experiment_id,
            config_.run_id,
            context_.next_iteration,
            config_.side,
            config_.algorithm,
            config_.payload_bytes,
            operation,
            duration_ns};
}

}  // namespace tcc::benchmark
