#pragma once

#include <cstdint>

#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/message_type.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::protocol {

[[nodiscard]] Bytes make_signed_content(MessageType type,
                                        AlgorithmId algorithm_id,
                                        std::uint32_t run_id,
                                        std::uint32_t iteration,
                                        const Bytes& payload);

}  // namespace tcc::protocol
