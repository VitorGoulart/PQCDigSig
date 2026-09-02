#include "tcc/network/socket_io.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <sys/socket.h>

#include "tcc/network/network_error.hpp"
#include "tcc/protocol/decoder.hpp"
#include "tcc/protocol/encoder.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::network {
namespace {

void require_open(const Socket& socket) {
    if (!socket.is_open()) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "I/O requires an open socket");
    }
}

}  // namespace

void read_exact(const Socket& socket,
                std::uint8_t* output,
                const std::size_t size) {
    require_open(socket);
    if (size != 0 && output == nullptr) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "read buffer must not be null");
    }

    std::size_t received = 0;
    while (received < size) {
        const auto result = ::recv(socket.native_handle(),
                                   output + received,
                                   size - received,
                                   0);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        if (result == 0) {
            throw NetworkError(NetworkErrorCode::peer_closed,
                               "peer closed the connection before all bytes were received");
        }
        if (errno != EINTR) {
            throw NetworkError(NetworkErrorCode::read_failed,
                               "could not read from socket",
                               errno);
        }
    }
}

void write_all(const Socket& socket,
               const std::uint8_t* data,
               const std::size_t size) {
    require_open(socket);
    if (size != 0 && data == nullptr) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "write buffer must not be null");
    }

    std::size_t sent = 0;
    while (sent < size) {
#ifdef MSG_NOSIGNAL
        constexpr int send_flags = MSG_NOSIGNAL;
#else
        constexpr int send_flags = 0;
#endif
        const auto result = ::send(socket.native_handle(),
                                   data + sent,
                                   size - sent,
                                   send_flags);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        if (result == 0) {
            throw NetworkError(NetworkErrorCode::peer_closed,
                               "peer closed the connection before all bytes were sent");
        }
        if (errno != EINTR) {
            throw NetworkError(NetworkErrorCode::write_failed,
                               "could not write to socket",
                               errno);
        }
    }
}

tcc::protocol::Bytes read_exact(const Socket& socket, const std::size_t size) {
    tcc::protocol::Bytes output(size);
    read_exact(socket, output.data(), output.size());
    return output;
}

void write_all(const Socket& socket, const tcc::protocol::Bytes& data) {
    write_all(socket, data.data(), data.size());
}

tcc::protocol::Message read_message(const Socket& socket) {
    std::array<std::uint8_t, tcc::protocol::limits::common_header_bytes>
        header_bytes{};
    read_exact(socket, header_bytes.data(), header_bytes.size());
    const auto header = tcc::protocol::decode_header(header_bytes.data(),
                                                     header_bytes.size());
    auto body = read_exact(socket, header.body_length);
    return tcc::protocol::decode_message(header, body);
}

void write_message(const Socket& socket,
                   const tcc::protocol::Message& message) {
    write_all(socket, tcc::protocol::encode_message(message));
}

}  // namespace tcc::network
