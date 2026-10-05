#include "tcc/server/server.hpp"

#include "tcc/crypto.hpp"
#include "tcc/csv.hpp"
#include "tcc/measurement.hpp"
#include "tcc/payload.hpp"
#include "tcc/transport.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tcc::server {
namespace {

constexpr std::array<std::size_t, 3> payload_test_order{
    payload_10_kib,
    payload_100_kib,
    payload_1_mib,
};

class Socket final {
public:
    explicit Socket(const int descriptor) : descriptor_(descriptor) {
        if (descriptor_ < 0) {
            throw std::system_error(
                errno, std::generic_category(), "failed to create socket");
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

[[noreturn]] void throw_socket_error(const std::string& operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

Socket create_listener() {
    Socket listener(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));

    const int reuse_address = 1;
    if (::setsockopt(
            listener.get(),
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)) != 0) {
        throw_socket_error("failed to configure the listening socket");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(tcp_port);
    if (::bind(
            listener.get(),
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0) {
        throw_socket_error(
            "failed to bind 0.0.0.0:" + std::to_string(tcp_port));
    }
    if (::listen(listener.get(), 1) != 0) {
        throw_socket_error("failed to listen for client connections");
    }
    return listener;
}

Socket accept_client(const Socket& listener) {
    for (;;) {
        const int descriptor = ::accept(listener.get(), nullptr, nullptr);
        if (descriptor >= 0) {
            return Socket(descriptor);
        }
        if (errno != EINTR) {
            throw_socket_error("failed to accept a client connection");
        }
    }
}

Measurement make_measurement(
    const ServerConfig& config,
    const std::uint32_t repetition,
    const Algorithm algorithm,
    const std::size_t payload_size,
    const Operation operation) {
    return {
        repetition,
        config.client,
        Side::server,
        algorithm,
        payload_size,
        operation,
        0,
        false,
    };
}

bool process_repetition(
    const int socket_fd,
    const ServerConfig& config,
    const std::uint32_t repetition,
    const std::size_t payload_size,
    const SignatureAlgorithm& algorithm,
    const Bytes& response_payload,
    MeasurementCsvWriter& writer) {
    Measurement keygen = make_measurement(
        config, repetition, algorithm.id(), payload_size, Operation::keygen);
    Measurement signing = make_measurement(
        config, repetition, algorithm.id(), payload_size, Operation::sign);
    Measurement verification = make_measurement(
        config, repetition, algorithm.id(), payload_size, Operation::verify);

    bool completed = false;
    try {
        const SignedPayload request =
            receive_signed_payload(socket_fd, payload_size, algorithm);

        const TimedResult<bool> verified = measure_verification(
            algorithm,
            request.payload,
            request.signature,
            request.public_key);
        verification.duration_ns = verified.duration_ns;
        verification.success = verified.value;
        if (!verified.value) {
            throw std::runtime_error("client signature verification failed");
        }

        TimedResult<KeyPair> keys = measure_key_generation(algorithm);
        keygen.duration_ns = keys.duration_ns;
        keygen.success = true;

        TimedResult<Bytes> signature = measure_signature(
            algorithm, response_payload, keys.value.private_key);
        signing.duration_ns = signature.duration_ns;
        signing.success = true;

        send_signed_payload(
            socket_fd,
            {response_payload, keys.value.public_key, signature.value},
            algorithm);
        completed = true;
    } catch (const std::exception& error) {
        std::cerr << "Repetition " << repetition << " (" << algorithm.name()
                  << ", " << payload_size << " bytes) failed: "
                  << error.what() << '\n';
    }

    writer.write(keygen);
    writer.write(signing);
    writer.write(verification);
    writer.flush();
    return completed;
}

}  // namespace

RunSummary run(const ServerConfig& config) {
    if (config.repetitions == 0U) {
        throw ConfigError("repetitions must be greater than zero");
    }
    if (config.client.empty()) {
        throw ConfigError("client label must not be empty");
    }
    if (config.output_path.empty()) {
        throw ConfigError("output path must not be empty");
    }

    initialize_crypto_runtime();
    Socket listener = create_listener();
    MeasurementCsvWriter writer(config.output_path);
    const std::uint64_t total =
        static_cast<std::uint64_t>(payload_test_order.size()) *
        algorithm_test_order.size() * config.repetitions;

    std::cout << "Listening on 0.0.0.0:" << tcp_port << " for " << total
              << " repetitions; writing " << config.output_path << '\n';

    RunSummary summary{};
    std::uint64_t current = 0U;
    for (const std::size_t payload_size : payload_test_order) {
        const Bytes response_payload = make_payload(payload_size);
        for (const Algorithm id : algorithm_test_order) {
            const std::unique_ptr<SignatureAlgorithm> algorithm =
                create_signature_algorithm(id);
            for (std::uint64_t repetition = 1U;
                 repetition <= config.repetitions;
                 ++repetition) {
                ++current;
                std::cout << '[' << current << '/' << total << "] waiting: "
                          << algorithm->name() << ", " << payload_size
                          << " bytes, repetition " << repetition << '\n';
                Socket client = accept_client(listener);
                if (process_repetition(
                        client.get(),
                        config,
                        static_cast<std::uint32_t>(repetition),
                        payload_size,
                        *algorithm,
                        response_payload,
                        writer)) {
                    ++summary.succeeded;
                } else {
                    ++summary.failed;
                }
            }
        }
    }
    return summary;
}

}  // namespace tcc::server
