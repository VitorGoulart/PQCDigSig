#pragma once

#include <cstddef>
#include <cstdint>

namespace tcc::protocol::limits {

inline constexpr std::size_t common_header_bytes = 5;
inline constexpr std::uint32_t payload_10_kib = 10U * 1024U;
inline constexpr std::uint32_t payload_100_kib = 100U * 1024U;
inline constexpr std::uint32_t payload_1_mib = 1024U * 1024U;
inline constexpr std::uint32_t max_payload_bytes = payload_1_mib;
inline constexpr std::uint32_t max_public_key_bytes = 16U * 1024U;
inline constexpr std::uint32_t max_signature_bytes = 16U * 1024U;
inline constexpr std::uint32_t max_body_bytes = 2U * 1024U * 1024U;
inline constexpr std::uint16_t max_error_description_bytes = 256;
inline constexpr std::uint32_t iterations_per_run = 1000;
inline constexpr std::uint32_t max_runs_per_connection = 1000;

}  // namespace tcc::protocol::limits
