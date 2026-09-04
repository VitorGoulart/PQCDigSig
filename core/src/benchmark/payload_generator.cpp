#include "tcc/benchmark/payload_generator.hpp"

#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::benchmark {
namespace {

using DigestPointer = std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)>;
using DigestContextPointer =
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

constexpr std::array<std::uint8_t, 14> payload_domain{
    'T', 'C', 'C', '-', 'P', 'A', 'Y', 'L', 'O', 'A', 'D', '-', 'v', '1'};

void store_u32_be(std::uint8_t* output, const std::uint32_t value) noexcept {
    output[0] = static_cast<std::uint8_t>(value >> 24U);
    output[1] = static_cast<std::uint8_t>(value >> 16U);
    output[2] = static_cast<std::uint8_t>(value >> 8U);
    output[3] = static_cast<std::uint8_t>(value);
}

[[nodiscard]] bool is_supported_payload(
    const std::uint32_t payload_bytes) noexcept {
    return payload_bytes == protocol::limits::payload_10_kib ||
           payload_bytes == protocol::limits::payload_100_kib ||
           payload_bytes == protocol::limits::payload_1_mib;
}

[[nodiscard]] std::uint8_t parse_nibble(const char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    throw std::invalid_argument("payload seed contains a non-hexadecimal digit");
}

[[nodiscard]] DigestPointer fetch_digest(const char* name) {
    DigestPointer digest(EVP_MD_fetch(nullptr, name, nullptr), &EVP_MD_free);
    if (!digest) {
        throw std::runtime_error(std::string("digest is unavailable: ") + name);
    }
    return digest;
}

[[nodiscard]] DigestContextPointer make_digest_context() {
    DigestContextPointer context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!context) {
        throw std::runtime_error("could not allocate a digest context");
    }
    return context;
}

}  // namespace

PayloadSeed parse_payload_seed(const std::string_view seed_hex) {
    if (seed_hex.size() != payload_seed_bytes * 2U) {
        throw std::invalid_argument(
            "payload seed must contain exactly 64 hexadecimal digits");
    }

    PayloadSeed seed{};
    for (std::size_t index = 0; index < seed.size(); ++index) {
        const std::uint8_t high = parse_nibble(seed_hex[index * 2U]);
        const std::uint8_t low = parse_nibble(seed_hex[index * 2U + 1U]);
        seed[index] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    return seed;
}

std::string payload_seed_to_hex(const PayloadSeed& seed) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output(seed.size() * 2U, '0');
    for (std::size_t index = 0; index < seed.size(); ++index) {
        output[index * 2U] = digits[seed[index] >> 4U];
        output[index * 2U + 1U] = digits[seed[index] & 0x0FU];
    }
    return output;
}

PayloadGenerator::PayloadGenerator(const PayloadSeed master_seed)
    : master_seed_(master_seed) {}

const PayloadSeed& PayloadGenerator::master_seed() const noexcept {
    return master_seed_;
}

PayloadSeed PayloadGenerator::derive_scenario_seed(
    const std::uint32_t experiment_id,
    const std::uint32_t repetition,
    const std::uint32_t payload_bytes) const {
    if (experiment_id == 0 || repetition == 0) {
        throw std::invalid_argument(
            "experiment ID and repetition must be positive");
    }
    if (!is_supported_payload(payload_bytes)) {
        throw std::invalid_argument("unsupported payload size");
    }

    std::array<std::uint8_t, payload_seed_bytes + 12U> input{};
    std::copy(master_seed_.begin(), master_seed_.end(), input.begin());
    store_u32_be(input.data() + payload_seed_bytes, experiment_id);
    store_u32_be(input.data() + payload_seed_bytes + 4U, repetition);
    store_u32_be(input.data() + payload_seed_bytes + 8U, payload_bytes);

    const auto digest = fetch_digest("SHA256");
    auto context = make_digest_context();
    PayloadSeed scenario_seed{};
    unsigned int output_size = 0;
    if (EVP_DigestInit_ex2(context.get(), digest.get(), nullptr) != 1 ||
        EVP_DigestUpdate(context.get(), input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(context.get(), scenario_seed.data(), &output_size) !=
            1 ||
        output_size != static_cast<unsigned int>(scenario_seed.size())) {
        throw std::runtime_error("SHA-256 payload seed derivation failed");
    }
    return scenario_seed;
}

crypto::Bytes PayloadGenerator::generate(
    const std::uint32_t experiment_id,
    const std::uint32_t repetition,
    const std::uint32_t payload_bytes) const {
    const PayloadSeed scenario_seed =
        derive_scenario_seed(experiment_id, repetition, payload_bytes);
    crypto::Bytes payload(payload_bytes);

    const auto digest = fetch_digest("SHAKE256");
    auto context = make_digest_context();
    if (EVP_DigestInit_ex2(context.get(), digest.get(), nullptr) != 1 ||
        EVP_DigestUpdate(
            context.get(), payload_domain.data(), payload_domain.size()) != 1 ||
        EVP_DigestUpdate(
            context.get(), scenario_seed.data(), scenario_seed.size()) != 1 ||
        EVP_DigestFinalXOF(context.get(), payload.data(), payload.size()) != 1) {
        throw std::runtime_error("SHAKE-256 payload expansion failed");
    }
    return payload;
}

}  // namespace tcc::benchmark
