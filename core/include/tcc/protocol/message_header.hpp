#pragma once

#include <cstdint>

#include "tcc/protocol/message_type.hpp"

namespace tcc::protocol {

struct MessageHeader {
    MessageType message_type;
    std::uint32_t body_length;
};

}  // namespace tcc::protocol
