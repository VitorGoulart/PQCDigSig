#include "tcc/protocol/protocol_error.hpp"

#include <utility>

namespace tcc::protocol {

ProtocolError::ProtocolError(const ErrorCode code, std::string message)
    : std::runtime_error(std::move(message)), code_(code) {}

ErrorCode ProtocolError::code() const noexcept { return code_; }

}  // namespace tcc::protocol
