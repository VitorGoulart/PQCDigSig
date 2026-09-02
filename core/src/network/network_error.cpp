#include "tcc/network/network_error.hpp"

#include <system_error>
#include <utility>

namespace tcc::network {
namespace {

std::string describe(std::string message, const int system_error_code) {
    if (system_error_code == 0) {
        return message;
    }
    message += ": ";
    message += std::error_code(system_error_code, std::generic_category())
                   .message();
    return message;
}

}  // namespace

NetworkError::NetworkError(const NetworkErrorCode code,
                           std::string message,
                           const int system_error_code)
    : std::runtime_error(describe(std::move(message), system_error_code)),
      code_(code),
      system_error_code_(system_error_code) {}

NetworkErrorCode NetworkError::code() const noexcept { return code_; }

int NetworkError::system_error_code() const noexcept {
    return system_error_code_;
}

}  // namespace tcc::network
