#include "tcc/measurement.hpp"
#include "tcc/timer.hpp"

#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void exercise_runtime_initialization() {
    std::promise<void> begin;
    const auto start = begin.get_future().share();
    const auto prepare_and_sign = [start] {
        start.wait();
        tcc::initialize_crypto_runtime();
        tcc::initialize_crypto_runtime();
        const auto algorithm =
            tcc::create_signature_algorithm(tcc::Algorithm::ed25519);
        const auto keys = algorithm->generate_key_pair();
        const tcc::Bytes payload{'r', 'u', 'n', 't', 'i', 'm', 'e'};
        const auto signature = algorithm->sign(payload, keys.private_key);
        tcc::initialize_crypto_runtime();
        expect(algorithm->verify(payload, signature, keys.public_key),
               "repeated initialization invalidated a signature");
        auto altered = payload;
        altered.front() ^= 0x01U;
        expect(!algorithm->verify(altered, signature, keys.public_key),
               "initialized worker accepted altered data");
        return keys.public_key;
    };
    auto first = std::async(std::launch::async, prepare_and_sign);
    auto second = std::async(std::launch::async, prepare_and_sign);
    begin.set_value();
    expect(first.get() != second.get(),
           "independent workers generated the same key");

    // A new measuring thread must also prepare its own random generators.
    tcc::initialize_crypto_runtime();
    tcc::initialize_crypto_runtime();
}

void exercise_measurements(const tcc::Algorithm id) {
    const std::unique_ptr<tcc::SignatureAlgorithm> algorithm =
        tcc::create_signature_algorithm(id);
    const tcc::Bytes payload(128U, 0x65U);

    const tcc::TimedResult<tcc::KeyPair> key_generation =
        tcc::measure_key_generation(*algorithm);
    expect(
        !key_generation.value.public_key.empty(),
        "measured key generation returned an empty public key");
    expect(
        !key_generation.value.private_key.empty(),
        "measured key generation returned an empty private key");
    expect(
        key_generation.duration_ns >= 0,
        "key-generation duration is negative");

    const tcc::TimedResult<tcc::Bytes> signing = tcc::measure_signature(
        *algorithm, payload, key_generation.value.private_key);
    expect(
        !signing.value.empty(),
        "measured signing returned an empty signature");
    expect(signing.duration_ns >= 0, "signing duration is negative");

    tcc::initialize_crypto_runtime();
    const tcc::TimedResult<bool> verification = tcc::measure_verification(
        *algorithm,
        payload,
        signing.value,
        key_generation.value.public_key);
    expect(verification.value, "measured verification rejected a signature");
    expect(
        verification.duration_ns >= 0,
        "verification duration is negative");

    tcc::Bytes altered_payload = payload;
    altered_payload.front() ^= 0x01U;
    const tcc::TimedResult<bool> rejected = tcc::measure_verification(
        *algorithm,
        altered_payload,
        signing.value,
        key_generation.value.public_key);
    expect(!rejected.value, "measured verification accepted altered data");
    expect(
        rejected.duration_ns >= 0,
        "rejected-verification duration is negative");
}

}  // namespace

int main() {
    try {
        static_assert(tcc::Timer::Clock::is_steady);
        exercise_runtime_initialization();
        for (const tcc::Algorithm algorithm : tcc::algorithm_test_order) {
            exercise_measurements(algorithm);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
