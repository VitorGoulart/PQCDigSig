#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "tcc/benchmark/payload_generator.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "../protocol/test_support.hpp"

namespace {

template <typename Function>
void expect_invalid_argument(Function&& function, const std::string& message) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_seed_encoding() {
    const std::string seed_hex =
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f";
    const auto seed = tcc::benchmark::parse_payload_seed(seed_hex);
    expect(tcc::benchmark::payload_seed_to_hex(seed) == seed_hex,
           "payload seed did not survive hexadecimal round trip");
    expect(tcc::benchmark::parse_payload_seed(
               "000102030405060708090A0B0C0D0E0F"
               "101112131415161718191A1B1C1D1E1F") == seed,
           "uppercase hexadecimal seed was not accepted");

    expect_invalid_argument(
        [] { static_cast<void>(tcc::benchmark::parse_payload_seed("00")); },
        "short payload seed was accepted");
    expect_invalid_argument(
        [] {
            static_cast<void>(tcc::benchmark::parse_payload_seed(
                "g00102030405060708090a0b0c0d0e0f"
                "101112131415161718191a1b1c1d1e1f"));
        },
        "non-hexadecimal payload seed was accepted");
}

void test_normative_vector_and_reproducibility() {
    const auto master_seed = tcc::benchmark::parse_payload_seed(
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f");
    const tcc::benchmark::PayloadGenerator generator(master_seed);
    expect(generator.master_seed() == master_seed,
           "generator did not retain its master seed");

    const auto scenario_seed = generator.derive_scenario_seed(
        1, 2, tcc::protocol::limits::payload_10_kib);
    expect(tcc::benchmark::payload_seed_to_hex(scenario_seed) ==
               "6a1c8508c503bf833e5ae8ab3b01f426"
               "b5fb7a65fd2e03c7b67fcc95adf15002",
           "SHA-256 scenario seed does not match the normative vector");

    const auto first = generator.generate(
        1, 2, tcc::protocol::limits::payload_10_kib);
    const auto second = generator.generate(
        1, 2, tcc::protocol::limits::payload_10_kib);
    expect(first == second, "same inputs generated different payloads");
    expect(first.size() == tcc::protocol::limits::payload_10_kib,
           "generated payload has the wrong size");

    tcc::benchmark::PayloadSeed prefix{};
    std::copy_n(first.begin(), prefix.size(), prefix.begin());
    expect(tcc::benchmark::payload_seed_to_hex(prefix) ==
               "c0eb74b1fec2f11fd0e79626405ae5f48"
               "3c3a0bd77950d0cf8747a9716a262d3",
           "SHAKE-256 payload does not match the normative vector");

    expect(first != generator.generate(
                        2, 2, tcc::protocol::limits::payload_10_kib),
           "experiment ID did not domain-separate payloads");
    expect(first != generator.generate(
                        1, 3, tcc::protocol::limits::payload_10_kib),
           "repetition did not domain-separate payloads");
}

void test_supported_sizes_and_invalid_inputs() {
    tcc::benchmark::PayloadSeed seed{};
    const tcc::benchmark::PayloadGenerator generator(seed);
    expect(generator.generate(1, 1, tcc::protocol::limits::payload_100_kib)
               .size() == tcc::protocol::limits::payload_100_kib,
           "100 KiB payload was not generated");
    expect(generator.generate(1, 1, tcc::protocol::limits::payload_1_mib)
               .size() == tcc::protocol::limits::payload_1_mib,
           "1 MiB payload was not generated");

    expect_invalid_argument(
        [&] {
            static_cast<void>(generator.generate(
                0, 1, tcc::protocol::limits::payload_10_kib));
        },
        "zero experiment ID was accepted");
    expect_invalid_argument(
        [&] {
            static_cast<void>(generator.generate(
                1, 0, tcc::protocol::limits::payload_10_kib));
        },
        "zero repetition was accepted");
    expect_invalid_argument(
        [&] { static_cast<void>(generator.generate(1, 1, 4096)); },
        "unsupported payload size was accepted");
}

}  // namespace

int main() {
    try {
        test_seed_encoding();
        test_normative_vector_and_reproducibility();
        test_supported_sizes_and_invalid_inputs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
