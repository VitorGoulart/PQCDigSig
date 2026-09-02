#pragma once

#include <stdexcept>
#include <string>

#include "tcc/protocol/messages.hpp"

namespace tcc::protocol {

class ProtocolError : public std::runtime_error {
public:
    ProtocolError(ErrorCode code, std::string message);

    [[nodiscard]] ErrorCode code() const noexcept;

private:
    ErrorCode code_;
};

}  // namespace tcc::protocol
