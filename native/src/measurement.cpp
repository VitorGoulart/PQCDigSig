#include "tcc/measurement.hpp"

#include "tcc/timer.hpp"

#include <stdexcept>
#include <utility>

namespace tcc {

std::string_view operation_name(const Operation operation) {
    switch (operation) {
        case Operation::keygen:
            return "keygen";
        case Operation::sign:
            return "sign";
        case Operation::verify:
            return "verify";
    }
    throw std::invalid_argument("unsupported measurement operation");
}

std::string_view side_name(const Side side) {
    switch (side) {
        case Side::client:
            return "client";
        case Side::server:
            return "server";
    }
    throw std::invalid_argument("unsupported measurement side");
}

TimedResult<KeyPair> measure_key_generation(
    const SignatureAlgorithm& algorithm) {
    const Timer timer;
    KeyPair key_pair = algorithm.generate_key_pair();
    const std::int64_t duration_ns = timer.elapsed_nanoseconds();
    return {std::move(key_pair), duration_ns};
}

TimedResult<Bytes> measure_signature(
    const SignatureAlgorithm& algorithm,
    const Bytes& payload,
    const Bytes& private_key) {
    const Timer timer;
    Bytes signature = algorithm.sign(payload, private_key);
    const std::int64_t duration_ns = timer.elapsed_nanoseconds();
    return {std::move(signature), duration_ns};
}

TimedResult<bool> measure_verification(
    const SignatureAlgorithm& algorithm,
    const Bytes& payload,
    const Bytes& signature,
    const Bytes& public_key) {
    const Timer timer;
    const bool verified = algorithm.verify(payload, signature, public_key);
    const std::int64_t duration_ns = timer.elapsed_nanoseconds();
    return {verified, duration_ns};
}

}  // namespace tcc
