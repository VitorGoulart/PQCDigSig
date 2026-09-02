#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/message_type.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::protocol::detail {

[[nodiscard]] bool is_valid(AlgorithmId value) noexcept;
[[nodiscard]] bool is_valid(MessageType value) noexcept;
[[nodiscard]] bool is_valid(StartRunStatus value) noexcept;
[[nodiscard]] bool is_valid(EndRunStatus value) noexcept;
[[nodiscard]] bool is_valid(ErrorCode value) noexcept;
[[nodiscard]] bool is_valid_payload_length(std::uint32_t value) noexcept;
[[nodiscard]] bool is_valid_utf8(const std::string& value) noexcept;

void validate_message(const Message& message);

}  // namespace tcc::protocol::detail
