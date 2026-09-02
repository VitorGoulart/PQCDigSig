#include "tcc/protocol/signed_content.hpp"

#include "protocol_validation.hpp"
#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::protocol {
namespace {

void append_u32(Bytes& output, const std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}

}  // namespace

Bytes make_signed_content(const MessageType type,
                          const AlgorithmId algorithm_id,
                          const std::uint32_t run_id,
                          const std::uint32_t iteration,
                          const Bytes& payload) {
    if (type != MessageType::signed_request &&
        type != MessageType::signed_response) {
        throw ProtocolError(ErrorCode::unexpected_message,
                            "only signed request/response types can be signed");
    }
    if (!detail::is_valid(algorithm_id)) {
        throw ProtocolError(ErrorCode::invalid_algorithm,
                            "unknown algorithm identifier");
    }
    if (run_id == 0) {
        throw ProtocolError(ErrorCode::invalid_identifier,
                            "run_id must be greater than zero");
    }
    if (iteration >= limits::iterations_per_run) {
        throw ProtocolError(ErrorCode::invalid_body,
                            "iteration is outside the configured run");
    }
    if (payload.size() > limits::max_payload_bytes) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "payload exceeds the protocol limit");
    }

    Bytes output;
    output.reserve(14 + payload.size());
    output.push_back(static_cast<std::uint8_t>(type));
    output.push_back(static_cast<std::uint8_t>(algorithm_id));
    append_u32(output, run_id);
    append_u32(output, iteration);
    append_u32(output, static_cast<std::uint32_t>(payload.size()));
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

}  // namespace tcc::protocol
