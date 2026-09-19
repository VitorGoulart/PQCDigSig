#pragma once

#include "tcc/crypto.hpp"

#include <cstddef>

namespace tcc {

inline constexpr std::size_t payload_10_kib = 10U * 1024U;
inline constexpr std::size_t payload_100_kib = 100U * 1024U;
inline constexpr std::size_t payload_1_mib = 1024U * 1024U;

[[nodiscard]] bool is_supported_payload_size(std::size_t size) noexcept;
[[nodiscard]] Bytes make_payload(std::size_t size);

}  // namespace tcc
