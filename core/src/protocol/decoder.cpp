#include "tcc/protocol/decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "protocol_validation.hpp"
#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::protocol {
namespace {

[[noreturn]] void invalid_body(const char* message) {
    throw ProtocolError(ErrorCode::invalid_body, message);
}

class Cursor {
public:
    explicit Cursor(const Bytes& bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint8_t read_u8() {
        require(1);
        return bytes_[position_++];
    }

    [[nodiscard]] std::uint16_t read_u16() {
        require(2);
        const auto value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes_[position_]) << 8U) |
            static_cast<std::uint16_t>(bytes_[position_ + 1]));
        position_ += 2;
        return value;
    }

    [[nodiscard]] std::uint32_t read_u32() {
        require(4);
        const auto value =
            (static_cast<std::uint32_t>(bytes_[position_]) << 24U) |
            (static_cast<std::uint32_t>(bytes_[position_ + 1]) << 16U) |
            (static_cast<std::uint32_t>(bytes_[position_ + 2]) << 8U) |
            static_cast<std::uint32_t>(bytes_[position_ + 3]);
        position_ += 4;
        return value;
    }

    [[nodiscard]] Bytes read_bytes(const std::size_t size) {
        require(size);
        const auto begin = bytes_.begin() + static_cast<std::ptrdiff_t>(position_);
        const auto end = begin + static_cast<std::ptrdiff_t>(size);
        position_ += size;
        return Bytes(begin, end);
    }

    [[nodiscard]] std::string read_string(const std::size_t size) {
        const auto bytes = read_bytes(size);
        return std::string(bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - position_;
    }

    void require_finished() const {
        if (remaining() != 0) {
            invalid_body("message body contains trailing bytes");
        }
    }

private:
    void require(const std::size_t size) const {
        if (size > remaining()) {
            invalid_body("message body is truncated");
        }
    }

    const Bytes& bytes_;
    std::size_t position_ = 0;
};

void require_body_size(const Bytes& body, const std::size_t expected) {
    if (body.size() != expected) {
        invalid_body("message body has an invalid length");
    }
}

template <typename SignedMessage>
SignedMessage decode_signed_message(const Bytes& body) {
    if (body.size() < 20) {
        invalid_body("signed message body is shorter than its fixed fields");
    }

    Cursor cursor(body);
    SignedMessage message{};
    message.run_id = cursor.read_u32();
    message.iteration = cursor.read_u32();
    const auto public_key_length = cursor.read_u32();
    const auto signature_length = cursor.read_u32();
    const auto payload_length = cursor.read_u32();

    if (public_key_length > limits::max_public_key_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "public key exceeds the protocol limit");
    }
    if (signature_length > limits::max_signature_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "signature exceeds the protocol limit");
    }
    if (payload_length > limits::max_payload_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "payload exceeds the protocol limit");
    }

    const auto declared_variable_size =
        static_cast<std::size_t>(public_key_length) + signature_length +
        payload_length;
    if (cursor.remaining() != declared_variable_size) {
        invalid_body("signed message lengths do not match body_length");
    }

    message.public_key = cursor.read_bytes(public_key_length);
    message.signature = cursor.read_bytes(signature_length);
    message.payload = cursor.read_bytes(payload_length);
    cursor.require_finished();
    return message;
}

}  // namespace

MessageHeader decode_header(const std::uint8_t* data, const std::size_t size) {
    if (data == nullptr) {
        invalid_body("header data must not be null");
    }
    if (size != limits::common_header_bytes) {
        invalid_body("message header must contain exactly five bytes");
    }

    const auto type = static_cast<MessageType>(data[0]);
    if (!detail::is_valid(type)) {
        throw ProtocolError(ErrorCode::unexpected_message,
                            "unknown message type in header");
    }
    const auto body_length =
        (static_cast<std::uint32_t>(data[1]) << 24U) |
        (static_cast<std::uint32_t>(data[2]) << 16U) |
        (static_cast<std::uint32_t>(data[3]) << 8U) |
        static_cast<std::uint32_t>(data[4]);
    if (body_length > limits::max_body_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "message body exceeds the protocol limit");
    }
    return {type, body_length};
}

MessageHeader decode_header(const Bytes& bytes) {
    return decode_header(bytes.data(), bytes.size());
}

Message decode_body(const MessageType type, const Bytes& body) {
    if (!detail::is_valid(type)) {
        throw ProtocolError(ErrorCode::unexpected_message,
                            "cannot decode an unknown message type");
    }
    if (body.size() > limits::max_body_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "message body exceeds the protocol limit");
    }

    Message message;
    switch (type) {
        case MessageType::start_run: {
            require_body_size(body, 17);
            Cursor cursor(body);
            message = StartRun{cursor.read_u32(),
                               cursor.read_u32(),
                               static_cast<AlgorithmId>(cursor.read_u8()),
                               cursor.read_u32(),
                               cursor.read_u32()};
            break;
        }
        case MessageType::start_run_ack: {
            require_body_size(body, 5);
            Cursor cursor(body);
            message = StartRunAck{
                cursor.read_u32(),
                static_cast<StartRunStatus>(cursor.read_u8())};
            break;
        }
        case MessageType::signed_request:
            message = decode_signed_message<SignedRequest>(body);
            break;
        case MessageType::signed_response:
            message = decode_signed_message<SignedResponse>(body);
            break;
        case MessageType::end_run: {
            require_body_size(body, 8);
            Cursor cursor(body);
            message = EndRun{cursor.read_u32(), cursor.read_u32()};
            break;
        }
        case MessageType::end_run_ack: {
            require_body_size(body, 5);
            Cursor cursor(body);
            message = EndRunAck{
                cursor.read_u32(),
                static_cast<EndRunStatus>(cursor.read_u8())};
            break;
        }
        case MessageType::finish:
            require_body_size(body, 0);
            message = Finish{};
            break;
        case MessageType::error: {
            if (body.size() < 13) {
                invalid_body("ERROR body is shorter than its fixed fields");
            }
            Cursor cursor(body);
            ErrorMessage error{
                static_cast<ErrorCode>(cursor.read_u16()),
                static_cast<MessageType>(cursor.read_u8()),
                cursor.read_u32(),
                cursor.read_u32(),
                {}};
            const auto description_length = cursor.read_u16();
            if (description_length > limits::max_error_description_bytes) {
                throw ProtocolError(
                    ErrorCode::limit_exceeded,
                    "error description exceeds the protocol limit");
            }
            if (cursor.remaining() != description_length) {
                invalid_body("ERROR description length does not match body_length");
            }
            error.description = cursor.read_string(description_length);
            message = std::move(error);
            break;
        }
    }

    detail::validate_message(message);
    return message;
}

Message decode_message(const MessageHeader& header, const Bytes& body) {
    if (!detail::is_valid(header.message_type)) {
        throw ProtocolError(ErrorCode::unexpected_message,
                            "unknown message type in header");
    }
    if (header.body_length > limits::max_body_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "message body exceeds the protocol limit");
    }
    if (body.size() != header.body_length) {
        invalid_body("body size does not match the message header");
    }
    return decode_body(header.message_type, body);
}

Message decode_message(const Bytes& frame) {
    if (frame.size() < limits::common_header_bytes) {
        invalid_body("message frame is shorter than its header");
    }
    const auto header = decode_header(frame.data(), limits::common_header_bytes);
    if (frame.size() - limits::common_header_bytes != header.body_length) {
        invalid_body("frame size does not match body_length");
    }
    const auto begin = frame.begin() +
                       static_cast<std::ptrdiff_t>(limits::common_header_bytes);
    return decode_message(header, Bytes(begin, frame.end()));
}

}  // namespace tcc::protocol
