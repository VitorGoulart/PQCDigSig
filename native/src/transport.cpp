#include "tcc/transport.hpp"

#include <array>
#include <cerrno>
#include <string>
#include <system_error>

#include <sys/socket.h>

namespace tcc {
namespace {

[[noreturn]] void throw_transport_error(const char* operation) {
    const std::error_code error(errno, std::generic_category());
    throw TransportError(std::string(operation) + ": " + error.message());
}

void validate_socket(const int socket_fd) {
    if (socket_fd < 0) {
        throw std::invalid_argument("socket descriptor must be non-negative");
    }
}

void validate_message(
    const SignedPayload& message,
    const SignatureAlgorithm& algorithm) {
    if (message.payload.empty()) {
        throw std::invalid_argument("payload must not be empty");
    }
    if (message.public_key.size() != algorithm.public_key_size()) {
        throw std::invalid_argument("public key has an unexpected size");
    }
    if (message.signature.empty() ||
        message.signature.size() > algorithm.maximum_signature_size()) {
        throw std::invalid_argument("signature has an invalid size");
    }
}

void send_all(const int socket_fd, const Bytes& bytes) {
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
#ifdef MSG_NOSIGNAL
        constexpr int send_flags = MSG_NOSIGNAL;
#else
        constexpr int send_flags = 0;
#endif
        const ssize_t sent = ::send(
            socket_fd,
            bytes.data() + offset,
            bytes.size() - offset,
            send_flags);
        if (sent > 0) {
            offset += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent == 0) {
            throw TransportError("socket closed while sending data");
        }
        throw_transport_error("failed to send data");
    }
}

void receive_exact(const int socket_fd, Bytes& destination) {
    std::size_t offset = 0U;
    while (offset < destination.size()) {
        const ssize_t received = ::recv(
            socket_fd,
            destination.data() + offset,
            destination.size() - offset,
            0);
        if (received > 0) {
            offset += static_cast<std::size_t>(received);
            continue;
        }
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received == 0) {
            throw TransportError(
                "connection ended before the message was complete");
        }
        throw_transport_error("failed to receive data");
    }
}

Bytes receive_signature(
    const int socket_fd,
    const std::size_t maximum_size) {
    Bytes signature;
    signature.reserve(maximum_size);
    std::array<std::uint8_t, 4096U> buffer{};

    for (;;) {
        const ssize_t received =
            ::recv(socket_fd, buffer.data(), buffer.size(), 0);
        if (received > 0) {
            const std::size_t count = static_cast<std::size_t>(received);
            if (count > maximum_size - signature.size()) {
                throw TransportError(
                    "received signature exceeds the algorithm limit");
            }
            signature.insert(
                signature.end(), buffer.cbegin(), buffer.cbegin() + count);
            continue;
        }
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received == 0) {
            break;
        }
        throw_transport_error("failed to receive signature");
    }

    if (signature.empty()) {
        throw TransportError("connection ended without a signature");
    }
    return signature;
}

void finish_writing(const int socket_fd) {
    while (::shutdown(socket_fd, SHUT_WR) != 0) {
        if (errno == EINTR) {
            continue;
        }
        throw_transport_error("failed to finish socket writing");
    }
}

}  // namespace

void send_signed_payload(
    const int socket_fd,
    const SignedPayload& message,
    const SignatureAlgorithm& algorithm) {
    validate_socket(socket_fd);
    validate_message(message, algorithm);
    send_all(socket_fd, message.payload);
    send_all(socket_fd, message.public_key);
    send_all(socket_fd, message.signature);
    finish_writing(socket_fd);
}

SignedPayload receive_signed_payload(
    const int socket_fd,
    const std::size_t payload_size,
    const SignatureAlgorithm& algorithm) {
    validate_socket(socket_fd);
    if (payload_size == 0U) {
        throw std::invalid_argument("payload size must be greater than zero");
    }

    SignedPayload message{
        Bytes(payload_size),
        Bytes(algorithm.public_key_size()),
        {},
    };
    receive_exact(socket_fd, message.payload);
    receive_exact(socket_fd, message.public_key);
    message.signature =
        receive_signature(socket_fd, algorithm.maximum_signature_size());
    return message;
}

}  // namespace tcc
