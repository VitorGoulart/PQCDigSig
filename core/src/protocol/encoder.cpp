#include "tcc/protocol/encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "protocol_validation.hpp"
#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::protocol {
namespace {

void append_u8(Bytes& output, const std::uint8_t value) {
    output.push_back(value);
}

void append_u16(Bytes& output, const std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}

void append_u32(Bytes& output, const std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}

void append_bytes(Bytes& output, const Bytes& value) {
    output.insert(output.end(), value.begin(), value.end());
}

template <typename SignedMessage>
void encode_signed_message(Bytes& output, const SignedMessage& message) {
    append_u32(output, message.run_id);
    append_u32(output, message.iteration);
    append_u32(output, static_cast<std::uint32_t>(message.public_key.size()));
    append_u32(output, static_cast<std::uint32_t>(message.signature.size()));
    append_u32(output, static_cast<std::uint32_t>(message.payload.size()));
    append_bytes(output, message.public_key);
    append_bytes(output, message.signature);
    append_bytes(output, message.payload);
}

}  // namespace

Bytes encode_header(const MessageHeader& header) {
    if (!detail::is_valid(header.message_type)) {
        throw ProtocolError(ErrorCode::unexpected_message,
                            "cannot encode an unknown message type");
    }
    if (header.body_length > limits::max_body_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "message body exceeds the protocol limit");
    }

    Bytes output;
    output.reserve(limits::common_header_bytes);
    append_u8(output, static_cast<std::uint8_t>(header.message_type));
    append_u32(output, header.body_length);
    return output;
}

Bytes encode_body(const Message& message) {
    detail::validate_message(message);

    Bytes output;
    std::visit(
        [&output](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, StartRun>) {
                output.reserve(17);
                append_u32(output, value.experiment_id);
                append_u32(output, value.run_id);
                append_u8(output, static_cast<std::uint8_t>(value.algorithm_id));
                append_u32(output, value.payload_length);
                append_u32(output, value.iteration_count);
            } else if constexpr (std::is_same_v<T, StartRunAck>) {
                output.reserve(5);
                append_u32(output, value.run_id);
                append_u8(output, static_cast<std::uint8_t>(value.status));
            } else if constexpr (std::is_same_v<T, SignedRequest> ||
                                 std::is_same_v<T, SignedResponse>) {
                output.reserve(20 + value.public_key.size() +
                               value.signature.size() + value.payload.size());
                encode_signed_message(output, value);
            } else if constexpr (std::is_same_v<T, EndRun>) {
                output.reserve(8);
                append_u32(output, value.run_id);
                append_u32(output, value.completed_iterations);
            } else if constexpr (std::is_same_v<T, EndRunAck>) {
                output.reserve(5);
                append_u32(output, value.run_id);
                append_u8(output, static_cast<std::uint8_t>(value.status));
            } else if constexpr (std::is_same_v<T, ErrorMessage>) {
                output.reserve(13 + value.description.size());
                append_u16(output, static_cast<std::uint16_t>(value.error_code));
                append_u8(output,
                          static_cast<std::uint8_t>(value.related_message_type));
                append_u32(output, value.run_id);
                append_u32(output, value.iteration);
                append_u16(output,
                           static_cast<std::uint16_t>(value.description.size()));
                output.insert(output.end(), value.description.begin(),
                              value.description.end());
            }
        },
        message);

    if (output.size() > limits::max_body_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "message body exceeds the protocol limit");
    }
    return output;
}

Bytes encode_message(const Message& message) {
    auto body = encode_body(message);
    auto output = encode_header(
        {message_type(message), static_cast<std::uint32_t>(body.size())});
    output.reserve(output.size() + body.size());
    output.insert(output.end(), body.begin(), body.end());
    return output;
}

}  // namespace tcc::protocol
