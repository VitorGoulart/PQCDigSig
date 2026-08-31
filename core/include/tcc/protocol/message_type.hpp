#pragma once

#include <cstdint>

namespace tcc::protocol {

enum class MessageType : std::uint8_t {
    start_run = 0x01,
    start_run_ack = 0x02,
    signed_request = 0x03,
    signed_response = 0x04,
    end_run = 0x05,
    end_run_ack = 0x06,
    finish = 0x07,
    error = 0x08,
};

}  // namespace tcc::protocol
