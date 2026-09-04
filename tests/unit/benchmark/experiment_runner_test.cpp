#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "tcc/benchmark/experiment_runner.hpp"
#include "tcc/benchmark/timer.hpp"
#include "tcc/crypto/algorithm_factory.hpp"
#include "tcc/crypto/signature_algorithm.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "tcc/protocol/signed_content.hpp"
#include "../protocol/test_support.hpp"

namespace {

using tcc::benchmark::ExperimentRunner;
using tcc::benchmark::Operation;
using tcc::benchmark::ParticipantSide;
using tcc::benchmark::RunConfig;
using tcc::benchmark::RunState;
using tcc::crypto::Bytes;
using tcc::crypto::KeyPair;
using tcc::crypto::SignatureResult;
using tcc::protocol::AlgorithmId;

struct CallCounts {
    std::uint32_t keygen = 0;
    std::uint32_t sign = 0;
    std::uint32_t verify = 0;
};

class FakeSignatureAlgorithm final : public tcc::crypto::SignatureAlgorithm {
  public:
    explicit FakeSignatureAlgorithm(std::shared_ptr<CallCounts> counts)
        : counts_(std::move(counts)) {}

    [[nodiscard]] AlgorithmId id() const noexcept override {
        return AlgorithmId::ed25519;
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "fake-ed25519";
    }

    [[nodiscard]] std::string_view provider() const noexcept override {
        return "test";
    }

    [[nodiscard]] KeyPair generate_key_pair() const override {
        ++counts_->keygen;
        const auto key = static_cast<std::uint8_t>(counts_->keygen & 0xFFU);
        return {{key}, {key}};
    }

    [[nodiscard]] SignatureResult sign(
        const Bytes& message,
        const Bytes& private_key) const override {
        ++counts_->sign;
        if (private_key.size() != 1) {
            throw std::runtime_error("invalid fake private key");
        }
        Bytes signature = message;
        signature.push_back(private_key.front());
        return {std::move(signature)};
    }

    [[nodiscard]] bool verify(const Bytes& message,
                              const Bytes& signature,
                              const Bytes& public_key) const override {
        ++counts_->verify;
        if (public_key.size() != 1) {
            throw std::runtime_error("invalid fake public key");
        }
        Bytes expected = message;
        expected.push_back(public_key.front());
        return signature == expected;
    }

  private:
    std::shared_ptr<CallCounts> counts_;
};

template <typename Exception, typename Function>
void expect_exception(Function&& function, const std::string& message) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

[[nodiscard]] RunConfig make_config(const ParticipantSide side) {
    return {17,
            4,
            AlgorithmId::ed25519,
            tcc::protocol::limits::payload_10_kib,
            side};
}

[[nodiscard]] ExperimentRunner make_runner(
    const ParticipantSide side,
    const std::shared_ptr<CallCounts>& counts) {
    return ExperimentRunner(
        make_config(side), std::make_unique<FakeSignatureAlgorithm>(counts));
}

void test_configuration_validation() {
    auto config = make_config(ParticipantSide::client);
    tcc::benchmark::validate_run_config(config);

    config.experiment_id = 0;
    expect_exception<std::invalid_argument>(
        [&] { tcc::benchmark::validate_run_config(config); },
        "zero experiment ID was accepted");
    config = make_config(ParticipantSide::client);
    config.algorithm = AlgorithmId::invalid;
    expect_exception<std::invalid_argument>(
        [&] { tcc::benchmark::validate_run_config(config); },
        "invalid algorithm was accepted");
    config = make_config(ParticipantSide::client);
    config.payload_bytes = 42;
    expect_exception<std::invalid_argument>(
        [&] { tcc::benchmark::validate_run_config(config); },
        "invalid payload size was accepted");
    config = make_config(ParticipantSide::client);
    config.warmup_iterations = 19;
    expect_exception<std::invalid_argument>(
        [&] { tcc::benchmark::validate_run_config(config); },
        "non-normative warmup count was accepted");
    config = make_config(ParticipantSide::client);
    config.measured_iterations = 999;
    expect_exception<std::invalid_argument>(
        [&] { tcc::benchmark::validate_run_config(config); },
        "non-normative measured iteration count was accepted");
}

void test_warmup_and_client_order() {
    const auto counts = std::make_shared<CallCounts>();
    auto runner = make_runner(ParticipantSide::client, counts);
    const Bytes content{0x01, 0x02, 0x03};

    expect(runner.context().state == RunState::awaiting_warmup,
           "new runner has an unexpected state");
    expect_exception<std::logic_error>(
        [&] { runner.begin_iteration(0); },
        "iteration began before warmup");

    runner.warm_up(content);
    expect(runner.context().state == RunState::ready,
           "runner was not ready after warmup");
    expect(counts->keygen == 20 && counts->sign == 20 && counts->verify == 20,
           "warmup did not execute exactly 20 complete operations");

    runner.begin_iteration(0);
    const auto outbound = runner.sign_outbound(content);
    expect(runner.verify_inbound(content,
                                 outbound.signature,
                                 outbound.public_key),
           "valid inbound signature was rejected");
    const auto measurements = runner.finish_iteration();

    expect(counts->keygen == 21 && counts->sign == 21 && counts->verify == 21,
           "measured iteration operation counts are incorrect");
    expect(runner.context().state == RunState::ready &&
               runner.context().next_iteration == 1 &&
               runner.context().completed_iterations == 1,
           "runner did not advance after the measured iteration");

    const auto rows = measurements.as_array();
    expect(rows[0].operation == Operation::key_generation &&
               rows[1].operation == Operation::signing &&
               rows[2].operation == Operation::verification,
           "measurements are not in normative operation order");
    for (const auto& row : rows) {
        expect(row.experiment_id == 17 && row.run_id == 4 &&
                   row.iteration == 0,
               "measurement identifiers are incorrect");
        expect(row.side == ParticipantSide::client &&
                   row.algorithm == AlgorithmId::ed25519 &&
                   row.payload_bytes ==
                       tcc::protocol::limits::payload_10_kib,
               "measurement metadata is incorrect");
        expect(row.duration_ns >= 0, "measurement duration is negative");
    }
    expect(tcc::benchmark::to_string(rows[0].side) == "client" &&
               tcc::benchmark::to_string(rows[0].operation) == "keygen" &&
               tcc::benchmark::to_string(rows[1].operation) == "sign" &&
               tcc::benchmark::to_string(rows[2].operation) == "verify",
           "CSV labels do not match the experimental specification");
}

void test_server_order_and_invalid_signature() {
    const auto counts = std::make_shared<CallCounts>();
    auto runner = make_runner(ParticipantSide::server, counts);
    const Bytes content{0xA5};
    runner.warm_up(content);
    runner.begin_iteration(0);

    const Bytes peer_key{0x3C};
    const Bytes peer_signature{0xA5, 0x3C};
    expect(runner.verify_inbound(content, peer_signature, peer_key),
           "server-order verification failed");
    static_cast<void>(runner.sign_outbound(content));
    const auto measurements = runner.finish_iteration();
    expect(measurements.verification.side == ParticipantSide::server,
           "server side was not recorded");

    runner.begin_iteration(1);
    static_cast<void>(runner.sign_outbound(content));
    Bytes invalid_signature = peer_signature;
    invalid_signature.back() ^= 0x01U;
    expect(!runner.verify_inbound(content, invalid_signature, peer_key),
           "invalid inbound signature was accepted");
    expect(runner.context().state == RunState::failed,
           "invalid signature did not fail the run");
    expect_exception<std::logic_error>(
        [&] { static_cast<void>(runner.finish_iteration()); },
        "failed iteration emitted partial measurements");
}

void test_state_guards_and_completion() {
    const auto counts = std::make_shared<CallCounts>();
    auto runner = make_runner(ParticipantSide::client, counts);
    const Bytes content{0x7E};
    runner.warm_up(content);

    expect_exception<std::invalid_argument>(
        [&] { runner.begin_iteration(1); },
        "out-of-order iteration was accepted");

    for (std::uint32_t iteration = 0;
         iteration < tcc::protocol::limits::iterations_per_run;
         ++iteration) {
        runner.begin_iteration(iteration);
        const auto artifact = runner.sign_outbound(content);
        expect(runner.verify_inbound(
                   content, artifact.signature, artifact.public_key),
               "valid signature failed during complete run");
        static_cast<void>(runner.finish_iteration());
    }

    expect(runner.context().state == RunState::completed &&
               runner.context().completed_iterations == 1000 &&
               runner.context().next_iteration == 1000,
           "runner did not complete exactly 1000 iterations");
    expect_exception<std::logic_error>(
        [&] { runner.begin_iteration(1000); },
        "completed run accepted another iteration");
    runner.abort();
    expect(runner.context().state == RunState::completed,
           "abort changed a completed run");
}

void test_timer_contract() {
    const tcc::benchmark::Timer::TimePoint start{};
    const auto end = start + std::chrono::nanoseconds(7);
    expect(tcc::benchmark::Timer::elapsed_nanoseconds(start, end) == 7,
           "timer did not convert the interval to nanoseconds");
    expect_exception<std::logic_error>(
        [&] {
            static_cast<void>(
                tcc::benchmark::Timer::elapsed_nanoseconds(end, start));
        },
        "timer accepted a reversed interval");
}

void test_real_algorithm_and_canonical_content() {
    auto runner = ExperimentRunner(make_config(ParticipantSide::client));
    const Bytes payload(tcc::protocol::limits::payload_10_kib, 0xA5);
    const Bytes outbound_content = tcc::protocol::make_signed_content(
        tcc::protocol::MessageType::signed_request,
        AlgorithmId::ed25519,
        4,
        0,
        payload);
    const Bytes inbound_content = tcc::protocol::make_signed_content(
        tcc::protocol::MessageType::signed_response,
        AlgorithmId::ed25519,
        4,
        0,
        payload);

    runner.warm_up(outbound_content);
    runner.begin_iteration(0);
    const auto outbound = runner.sign_outbound(outbound_content);

    auto peer = tcc::crypto::create_signature_algorithm(AlgorithmId::ed25519);
    const auto peer_key_pair = peer->generate_key_pair();
    const auto peer_signature =
        peer->sign(inbound_content, peer_key_pair.private_key);
    expect(runner.verify_inbound(inbound_content,
                                 peer_signature.signature,
                                 peer_key_pair.public_key),
           "runner rejected a signature from the real adapter");
    const auto measurements = runner.finish_iteration();
    expect(measurements.key_generation.duration_ns >= 0 &&
               measurements.signing.duration_ns >= 0 &&
               measurements.verification.duration_ns >= 0,
           "real adapter produced a negative duration");
    expect(!outbound.public_key.empty() && !outbound.signature.empty(),
           "real adapter produced empty outbound artifacts");
}

}  // namespace

int main() {
    try {
        test_configuration_validation();
        test_warmup_and_client_order();
        test_server_order_and_invalid_signature();
        test_state_guards_and_completion();
        test_timer_contract();
        test_real_algorithm_and_canonical_content();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
