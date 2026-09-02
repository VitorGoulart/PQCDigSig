#pragma once

#include <stdexcept>
#include <string>

namespace tcc::network {

enum class NetworkErrorCode {
    invalid_argument,
    resolution_failed,
    socket_creation_failed,
    connection_failed,
    bind_failed,
    listen_failed,
    accept_failed,
    read_failed,
    write_failed,
    shutdown_failed,
    peer_closed,
};

class NetworkError : public std::runtime_error {
public:
    NetworkError(NetworkErrorCode code,
                 std::string message,
                 int system_error_code = 0);

    [[nodiscard]] NetworkErrorCode code() const noexcept;
    [[nodiscard]] int system_error_code() const noexcept;

private:
    NetworkErrorCode code_;
    int system_error_code_;
};

}  // namespace tcc::network
