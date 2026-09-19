#include "tcc/payload.hpp"
#include "tcc/transport.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class Socket final {
public:
    explicit Socket(const int descriptor) : descriptor_(descriptor) {
        if (descriptor_ < 0) {
            throw std::system_error(
                errno, std::generic_category(), "failed to create test socket");
        }
    }

    ~Socket() {
        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept
        : descriptor_(std::exchange(other.descriptor_, -1)) {}

    Socket& operator=(Socket&&) = delete;

    [[nodiscard]] int get() const noexcept {
        return descriptor_;
    }

private:
    int descriptor_;
};

struct ConnectedSockets {
    Socket client;
    Socket server;
};

ConnectedSockets make_tcp_connection() {
    Socket listener{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(
            listener.get(), reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0 ||
        ::listen(listener.get(), 1) != 0) {
        throw std::runtime_error("failed to prepare loopback TCP listener");
    }

    socklen_t address_size = sizeof(address);
    if (::getsockname(
            listener.get(), reinterpret_cast<sockaddr*>(&address),
            &address_size) != 0) {
        throw std::runtime_error("failed to inspect loopback TCP listener");
    }

    Socket client{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    while (::connect(
               client.get(), reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0) {
        if (errno == EINTR) {
            continue;
        }
        throw std::runtime_error("failed to connect loopback TCP client");
    }

    int accepted = -1;
    do {
        accepted = ::accept(listener.get(), nullptr, nullptr);
    } while (accepted < 0 && errno == EINTR);
    return {std::move(client), Socket{accepted}};
}

void send_in_fragments(
    const int socket_fd,
    const tcc::Bytes& bytes,
    const std::size_t fragment_size) {
    std::size_t offset = 0U;
    while (offset < bytes.size()) {
        const std::size_t requested =
            std::min(fragment_size, bytes.size() - offset);
#ifdef MSG_NOSIGNAL
        constexpr int send_flags = MSG_NOSIGNAL;
#else
        constexpr int send_flags = 0;
#endif
        const ssize_t sent = ::send(
            socket_fd, bytes.data() + offset, requested, send_flags);
        if (sent > 0) {
            offset += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        throw std::runtime_error("failed to send fragmented test data");
    }
}

void finish_raw_writing(const int socket_fd) {
    while (::shutdown(socket_fd, SHUT_WR) != 0) {
        if (errno != EINTR) {
            throw std::runtime_error("failed to finish fragmented test data");
        }
    }
}

template <typename Callable>
void expect_transport_error(Callable&& callable) {
    try {
        callable();
        throw std::runtime_error("expected tcc::TransportError");
    } catch (const tcc::TransportError&) {
    }
}

void verify_message(
    const tcc::SignedPayload& expected,
    const tcc::SignedPayload& actual,
    const tcc::SignatureAlgorithm& algorithm) {
    expect(actual.payload == expected.payload, "received payload differs");
    expect(
        actual.public_key == expected.public_key,
        "received public key differs");
    expect(
        actual.signature == expected.signature,
        "received signature differs");
    expect(
        algorithm.verify(
            actual.payload, actual.signature, actual.public_key),
        "received signature is not valid");
}

}  // namespace

int main() {
    try {
        expect(tcc::tcp_port == 62037U, "unexpected fixed TCP port");

        const std::unique_ptr<tcc::SignatureAlgorithm> algorithm =
            tcc::create_signature_algorithm(tcc::Algorithm::ed25519);
        const tcc::Bytes payload = tcc::make_payload(tcc::payload_10_kib);
        const tcc::KeyPair client_keys = algorithm->generate_key_pair();
        const tcc::SignedPayload client_message{
            payload,
            client_keys.public_key,
            algorithm->sign(payload, client_keys.private_key),
        };

        ConnectedSockets bidirectional = make_tcp_connection();
        tcc::send_signed_payload(
            bidirectional.client.get(), client_message, *algorithm);
        const tcc::SignedPayload received_by_server =
            tcc::receive_signed_payload(
                bidirectional.server.get(), payload.size(), *algorithm);
        verify_message(client_message, received_by_server, *algorithm);

        const tcc::KeyPair server_keys = algorithm->generate_key_pair();
        const tcc::SignedPayload server_message{
            payload,
            server_keys.public_key,
            algorithm->sign(payload, server_keys.private_key),
        };
        tcc::send_signed_payload(
            bidirectional.server.get(), server_message, *algorithm);
        const tcc::SignedPayload received_by_client =
            tcc::receive_signed_payload(
                bidirectional.client.get(), payload.size(), *algorithm);
        verify_message(server_message, received_by_client, *algorithm);

        ConnectedSockets fragmented = make_tcp_connection();
        send_in_fragments(fragmented.client.get(), client_message.payload, 3U);
        send_in_fragments(
            fragmented.client.get(), client_message.public_key, 2U);
        send_in_fragments(
            fragmented.client.get(), client_message.signature, 1U);
        finish_raw_writing(fragmented.client.get());
        const tcc::SignedPayload reassembled = tcc::receive_signed_payload(
            fragmented.server.get(), payload.size(), *algorithm);
        verify_message(client_message, reassembled, *algorithm);

        ConnectedSockets truncated = make_tcp_connection();
        send_in_fragments(
            truncated.client.get(), tcc::Bytes(128U, 0x65U), 7U);
        finish_raw_writing(truncated.client.get());
        expect_transport_error([&] {
            static_cast<void>(tcc::receive_signed_payload(
                truncated.server.get(), payload.size(), *algorithm));
        });

        ConnectedSockets oversized = make_tcp_connection();
        send_in_fragments(oversized.client.get(), client_message.payload, 13U);
        send_in_fragments(
            oversized.client.get(), client_message.public_key, 13U);
        send_in_fragments(
            oversized.client.get(),
            tcc::Bytes(algorithm->maximum_signature_size() + 1U, 0x65U),
            13U);
        finish_raw_writing(oversized.client.get());
        expect_transport_error([&] {
            static_cast<void>(tcc::receive_signed_payload(
                oversized.server.get(), payload.size(), *algorithm));
        });

        ConnectedSockets invalid = make_tcp_connection();
        tcc::SignedPayload invalid_message = client_message;
        invalid_message.public_key.pop_back();
        try {
            tcc::send_signed_payload(
                invalid.client.get(), invalid_message, *algorithm);
            throw std::runtime_error("expected std::invalid_argument");
        } catch (const std::invalid_argument&) {
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
