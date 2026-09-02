#pragma once

#include "tcc/protocol/message_header.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::protocol {

[[nodiscard]] Bytes encode_header(const MessageHeader& header);
[[nodiscard]] Bytes encode_body(const Message& message);
[[nodiscard]] Bytes encode_message(const Message& message);

}  // namespace tcc::protocol
