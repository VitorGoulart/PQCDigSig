#include "tcc/network/socket.hpp"

#include <cerrno>
#include <cstdint>
#include <string>
#include <utility>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tcc/network/network_error.hpp"

namespace tcc::network {
namespace {

void require_endpoint(const Endpoint& endpoint,
                      const bool allow_empty_host,
                      const bool allow_zero_port) {
    if ((!allow_empty_host && endpoint.host.empty()) ||
        (!allow_zero_port && endpoint.port == 0)) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "endpoint requires a host and a non-zero port");
    }
}

void require_open(const Socket& socket) {
    if (!socket.is_open()) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "operation requires an open socket");
    }
}

class AddressList {
public:
    AddressList(const Endpoint& endpoint, const bool passive) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = passive ? AI_PASSIVE : 0;

        const auto service = std::to_string(endpoint.port);
        const char* host = endpoint.host.empty() ? nullptr : endpoint.host.c_str();
        const int result =
            ::getaddrinfo(host, service.c_str(), &hints, &addresses_);
        if (result != 0) {
            throw NetworkError(NetworkErrorCode::resolution_failed,
                               std::string("could not resolve endpoint: ") +
                                   ::gai_strerror(result));
        }
    }

    ~AddressList() {
        if (addresses_ != nullptr) {
            ::freeaddrinfo(addresses_);
        }
    }

    AddressList(const AddressList&) = delete;
    AddressList& operator=(const AddressList&) = delete;

    [[nodiscard]] const addrinfo* get() const noexcept { return addresses_; }

private:
    addrinfo* addresses_ = nullptr;
};

}  // namespace

Socket::Socket(const NativeHandle handle) noexcept : handle_(handle) {}

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept
    : handle_(std::exchange(other.handle_, invalid_handle)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, invalid_handle);
    }
    return *this;
}

Socket Socket::adopt(const NativeHandle handle) {
    if (handle < 0) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "cannot adopt an invalid socket handle");
    }
    return Socket(handle);
}

Socket Socket::connect_to(const Endpoint& endpoint) {
    require_endpoint(endpoint, false, false);
    const AddressList addresses(endpoint, false);
    int last_error = 0;

    for (auto* address = addresses.get(); address != nullptr;
         address = address->ai_next) {
        const auto handle = ::socket(address->ai_family,
                                     address->ai_socktype,
                                     address->ai_protocol);
        if (handle == invalid_handle) {
            last_error = errno;
            continue;
        }
        if (::connect(handle, address->ai_addr, address->ai_addrlen) == 0) {
            return Socket(handle);
        }
        last_error = errno;
        ::close(handle);
    }

    throw NetworkError(NetworkErrorCode::connection_failed,
                       "could not connect to endpoint",
                       last_error);
}

Socket Socket::listen_on(const Endpoint& endpoint, const int backlog) {
    require_endpoint(endpoint, true, true);
    if (backlog <= 0) {
        throw NetworkError(NetworkErrorCode::invalid_argument,
                           "listen backlog must be greater than zero");
    }

    const AddressList addresses(endpoint, true);
    int last_error = 0;
    NetworkErrorCode last_code = NetworkErrorCode::socket_creation_failed;

    for (auto* address = addresses.get(); address != nullptr;
         address = address->ai_next) {
        const auto handle = ::socket(address->ai_family,
                                     address->ai_socktype,
                                     address->ai_protocol);
        if (handle == invalid_handle) {
            last_error = errno;
            continue;
        }

        const int enabled = 1;
        if (::setsockopt(handle,
                         SOL_SOCKET,
                         SO_REUSEADDR,
                         &enabled,
                         sizeof(enabled)) != 0) {
            last_error = errno;
            last_code = NetworkErrorCode::bind_failed;
            ::close(handle);
            continue;
        }
        if (::bind(handle, address->ai_addr, address->ai_addrlen) != 0) {
            last_error = errno;
            last_code = NetworkErrorCode::bind_failed;
            ::close(handle);
            continue;
        }
        if (::listen(handle, backlog) != 0) {
            last_error = errno;
            last_code = NetworkErrorCode::listen_failed;
            ::close(handle);
            continue;
        }
        return Socket(handle);
    }

    throw NetworkError(last_code,
                       "could not bind and listen on endpoint",
                       last_error);
}

Socket Socket::accept() const {
    require_open(*this);
    for (;;) {
        const auto accepted = ::accept(handle_, nullptr, nullptr);
        if (accepted != invalid_handle) {
            return Socket(accepted);
        }
        if (errno != EINTR) {
            throw NetworkError(NetworkErrorCode::accept_failed,
                               "could not accept TCP connection",
                               errno);
        }
    }
}

void Socket::shutdown_write() {
    require_open(*this);
    if (::shutdown(handle_, SHUT_WR) != 0) {
        throw NetworkError(NetworkErrorCode::shutdown_failed,
                           "could not shut down socket writes",
                           errno);
    }
}

void Socket::close() noexcept {
    if (handle_ != invalid_handle) {
        ::close(handle_);
        handle_ = invalid_handle;
    }
}

bool Socket::is_open() const noexcept { return handle_ >= 0; }

Socket::NativeHandle Socket::native_handle() const noexcept { return handle_; }

}  // namespace tcc::network
