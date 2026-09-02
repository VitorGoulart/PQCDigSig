#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/message_type.hpp"

namespace tcc::protocol {

using Bytes = std::vector<std::uint8_t>;

enum class StartRunStatus : std::uint8_t {
    accepted = 0,
    invalid_algorithm = 1,
    invalid_payload_length = 2,
    invalid_iteration_count = 3,
    invalid_or_repeated_id = 4,
    server_busy = 5,
};

enum class EndRunStatus : std::uint8_t {
    completed = 0,
    rejected = 1,
};

enum class ErrorCode : std::uint16_t {
    unexpected_message = 1,
    invalid_body = 2,
    limit_exceeded = 3,
    invalid_algorithm = 4,
    invalid_signature = 5,
    cryptographic_error = 6,
    invalid_identifier = 7,
    internal_error = 8,
};

struct StartRun {
    std::uint32_t experiment_id;
    std::uint32_t run_id;
    AlgorithmId algorithm_id;
    std::uint32_t payload_length;
    std::uint32_t iteration_count;
};

struct StartRunAck {
    std::uint32_t run_id;
    StartRunStatus status;
};

struct SignedRequest {
    std::uint32_t run_id;
    std::uint32_t iteration;
    Bytes public_key;
    Bytes signature;
    Bytes payload;
};

struct SignedResponse {
    std::uint32_t run_id;
    std::uint32_t iteration;
    Bytes public_key;
    Bytes signature;
    Bytes payload;
};

struct EndRun {
    std::uint32_t run_id;
    std::uint32_t completed_iterations;
};

struct EndRunAck {
    std::uint32_t run_id;
    EndRunStatus status;
};

struct Finish {};

struct ErrorMessage {
    ErrorCode error_code;
    MessageType related_message_type;
    std::uint32_t run_id;
    std::uint32_t iteration;
    std::string description;
};

using Message = std::variant<StartRun,
                             StartRunAck,
                             SignedRequest,
                             SignedResponse,
                             EndRun,
                             EndRunAck,
                             Finish,
                             ErrorMessage>;

[[nodiscard]] MessageType message_type(const Message& message) noexcept;

}  // namespace tcc::protocol
