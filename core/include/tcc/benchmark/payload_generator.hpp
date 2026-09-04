#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "tcc/crypto/key_pair.hpp"

namespace tcc::benchmark {

inline constexpr std::size_t payload_seed_bytes = 32;
using PayloadSeed = std::array<std::uint8_t, payload_seed_bytes>;

[[nodiscard]] PayloadSeed parse_payload_seed(std::string_view seed_hex);
[[nodiscard]] std::string payload_seed_to_hex(const PayloadSeed& seed);

class PayloadGenerator final {
  public:
    explicit PayloadGenerator(PayloadSeed master_seed);

    [[nodiscard]] const PayloadSeed& master_seed() const noexcept;
    [[nodiscard]] PayloadSeed derive_scenario_seed(
        std::uint32_t experiment_id,
        std::uint32_t repetition,
        std::uint32_t payload_bytes) const;
    [[nodiscard]] crypto::Bytes generate(std::uint32_t experiment_id,
                                         std::uint32_t repetition,
                                         std::uint32_t payload_bytes) const;

  private:
    PayloadSeed master_seed_;
};

}  // namespace tcc::benchmark
