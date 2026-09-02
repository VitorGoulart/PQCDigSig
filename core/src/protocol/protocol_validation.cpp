#include "protocol_validation.hpp"

#include <type_traits>

#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::protocol::detail {
namespace {

[[noreturn]] void invalid_body(const char* message) {
    throw ProtocolError(ErrorCode::invalid_body, message);
}

void validate_run_id(const std::uint32_t run_id) {
    if (run_id == 0) {
        throw ProtocolError(ErrorCode::invalid_identifier,
                            "run_id must be greater than zero");
    }
}

template <typename SignedMessage>
void validate_signed_message(const SignedMessage& message) {
    validate_run_id(message.run_id);
    if (message.iteration >= limits::iterations_per_run) {
        invalid_body("iteration is outside the configured run");
    }
    if (message.public_key.size() > limits::max_public_key_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "public key exceeds the protocol limit");
    }
    if (message.signature.size() > limits::max_signature_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "signature exceeds the protocol limit");
    }
    if (message.payload.size() > limits::max_payload_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "payload exceeds the protocol limit");
    }
}

}  // namespace

bool is_valid(const AlgorithmId value) noexcept {
    switch (value) {
        case AlgorithmId::ml_dsa_44:
        case AlgorithmId::ml_dsa_65:
        case AlgorithmId::ml_dsa_87:
        case AlgorithmId::falcon_512:
        case AlgorithmId::falcon_1024:
        case AlgorithmId::rsa_pss:
        case AlgorithmId::ed25519:
            return true;
        case AlgorithmId::invalid:
            return false;
    }
    return false;
}

bool is_valid(const MessageType value) noexcept {
    switch (value) {
        case MessageType::start_run:
        case MessageType::start_run_ack:
        case MessageType::signed_request:
        case MessageType::signed_response:
        case MessageType::end_run:
        case MessageType::end_run_ack:
        case MessageType::finish:
        case MessageType::error:
            return true;
    }
    return false;
}

bool is_valid(const StartRunStatus value) noexcept {
    switch (value) {
        case StartRunStatus::accepted:
        case StartRunStatus::invalid_algorithm:
        case StartRunStatus::invalid_payload_length:
        case StartRunStatus::invalid_iteration_count:
        case StartRunStatus::invalid_or_repeated_id:
        case StartRunStatus::server_busy:
            return true;
    }
    return false;
}

bool is_valid(const EndRunStatus value) noexcept {
    return value == EndRunStatus::completed || value == EndRunStatus::rejected;
}

bool is_valid(const ErrorCode value) noexcept {
    switch (value) {
        case ErrorCode::unexpected_message:
        case ErrorCode::invalid_body:
        case ErrorCode::limit_exceeded:
        case ErrorCode::invalid_algorithm:
        case ErrorCode::invalid_signature:
        case ErrorCode::cryptographic_error:
        case ErrorCode::invalid_identifier:
        case ErrorCode::internal_error:
            return true;
    }
    return false;
}

bool is_valid_payload_length(const std::uint32_t value) noexcept {
    return value == limits::payload_10_kib ||
           value == limits::payload_100_kib ||
           value == limits::payload_1_mib;
}

bool is_valid_utf8(const std::string& value) noexcept {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<std::uint8_t>(value[index]);
        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;

        if (first <= 0x7F) {
            ++index;
            continue;
        }
        if ((first & 0xE0U) == 0xC0U) {
            continuation_count = 1;
            code_point = first & 0x1FU;
            if (code_point < 2) {
                return false;
            }
        } else if ((first & 0xF0U) == 0xE0U) {
            continuation_count = 2;
            code_point = first & 0x0FU;
        } else if ((first & 0xF8U) == 0xF0U) {
            continuation_count = 3;
            code_point = first & 0x07U;
        } else {
            return false;
        }

        if (continuation_count > value.size() - index - 1) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto next = static_cast<std::uint8_t>(value[index + offset]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (next & 0x3FU);
        }

        if ((continuation_count == 2 && code_point < 0x800U) ||
            (continuation_count == 3 && code_point < 0x10000U) ||
            code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

void validate_message(const Message& message) {
    std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, StartRun>) {
                if (value.experiment_id == 0 || value.run_id == 0) {
                    throw ProtocolError(ErrorCode::invalid_identifier,
                                        "experiment_id and run_id must be greater than zero");
                }
                if (!is_valid(value.algorithm_id)) {
                    throw ProtocolError(ErrorCode::invalid_algorithm,
                                        "unknown algorithm identifier");
                }
                if (!is_valid_payload_length(value.payload_length)) {
                    invalid_body("unsupported payload length");
                }
                if (value.iteration_count != limits::iterations_per_run) {
                    invalid_body("iteration_count must be exactly 1000");
                }
            } else if constexpr (std::is_same_v<T, StartRunAck>) {
                validate_run_id(value.run_id);
                if (!is_valid(value.status)) {
                    invalid_body("unknown START_RUN_ACK status");
                }
            } else if constexpr (std::is_same_v<T, SignedRequest> ||
                                 std::is_same_v<T, SignedResponse>) {
                validate_signed_message(value);
            } else if constexpr (std::is_same_v<T, EndRun>) {
                validate_run_id(value.run_id);
                if (value.completed_iterations != limits::iterations_per_run) {
                    invalid_body("completed_iterations must be exactly 1000");
                }
            } else if constexpr (std::is_same_v<T, EndRunAck>) {
                validate_run_id(value.run_id);
                if (!is_valid(value.status)) {
                    invalid_body("unknown END_RUN_ACK status");
                }
            } else if constexpr (std::is_same_v<T, ErrorMessage>) {
                if (!is_valid(value.error_code)) {
                    invalid_body("unknown error code");
                }
                if (!is_valid(value.related_message_type)) {
                    invalid_body("unknown related message type");
                }
                if (value.description.size() >
                    limits::max_error_description_bytes) {
                    throw ProtocolError(ErrorCode::limit_exceeded,
                                        "error description exceeds the protocol limit");
                }
                if (!is_valid_utf8(value.description)) {
                    invalid_body("error description is not valid UTF-8");
                }
            }
        },
        message);
}

}  // namespace tcc::protocol::detail
