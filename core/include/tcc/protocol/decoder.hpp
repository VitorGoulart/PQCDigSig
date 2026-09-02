#pragma once

#include <cstddef>
#include <cstdint>

#include "tcc/protocol/message_header.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::protocol {

[[nodiscard]] MessageHeader decode_header(const std::uint8_t* data,
                                          std::size_t size);
[[nodiscard]] MessageHeader decode_header(const Bytes& bytes);
[[nodiscard]] Message decode_body(MessageType type, const Bytes& body);
[[nodiscard]] Message decode_message(const MessageHeader& header,
                                     const Bytes& body);
[[nodiscard]] Message decode_message(const Bytes& frame);

}  // namespace tcc::protocol
