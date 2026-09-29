#include "tcc/client.hpp"

#include "tcc/csv.hpp"
#include "tcc/payload.hpp"
#include "tcc/transport.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tcc::client {

void Control::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
    if (descriptor_ >= 0) ::shutdown(descriptor_, SHUT_RDWR);
}

void Control::attach(int descriptor) {
    std::lock_guard<std::mutex> lock(mutex_);
    descriptor_ = descriptor;
    if (cancelled_) ::shutdown(descriptor_, SHUT_RDWR);
}

void Control::detach() {
    std::lock_guard<std::mutex> lock(mutex_);
    descriptor_ = -1;
}

namespace {

void check_cancelled(const Control& control) {
    if (control.cancelled()) throw std::runtime_error("Execução cancelada.");
}

void check_socket(int result, const char* operation) {
    if (result < 0) throw std::system_error(errno, std::generic_category(), operation);
}

class Connection final {
public:
    explicit Connection(Control& control) : control_(control) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        check_socket(fd_, "socket");
        control_.attach(fd_);
    }
    ~Connection() {
        // Detach before close so cancellation never touches a recycled descriptor.
        control_.detach();
        ::close(fd_);
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    int get() const { return fd_; }

    void connect(const sockaddr_in& address) {
        check_cancelled(control_);
        const int flags = ::fcntl(fd_, F_GETFL, 0);
        check_socket(flags, "fcntl");
        check_socket(::fcntl(fd_, F_SETFL, flags | O_NONBLOCK), "fcntl");
        const int result = ::connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
        if (result < 0 && errno != EINPROGRESS && errno != EINTR) check_socket(result, "connect");
        if (result < 0) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            for (;;) {
                check_cancelled(control_);
                if (std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Tempo limite de conexão (10 s).");
                pollfd event{fd_, POLLOUT, 0};
                const int ready = ::poll(&event, 1, 100);
                if (ready < 0 && errno == EINTR) continue;
                check_socket(ready, "poll");
                if (ready == 0) continue;
                int error = 0;
                socklen_t size = sizeof(error);
                check_socket(::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &size), "getsockopt");
                if (error) throw std::system_error(error, std::generic_category(), "connect");
                break;
            }
        }
        check_cancelled(control_);
        check_socket(::fcntl(fd_, F_SETFL, flags), "fcntl");
        const timeval timeout{30, 0};
        check_socket(::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)), "receive timeout");
        check_socket(::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)), "send timeout");
    }
private:
    Control& control_;
    int fd_;
};

}  // namespace

Summary run(const Config& config, Control& control,
            const std::function<void(const Progress&)>& progress) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(tcp_port);
    if (::inet_pton(AF_INET, config.server_ipv4.c_str(), &address.sin_addr) != 1)
        throw std::invalid_argument("Informe um endereço IPv4 válido.");
    if (config.repetitions == 0 || config.client.empty() || config.output_path.empty())
        throw std::invalid_argument("Configuração do cliente inválida.");

    MeasurementCsvWriter writer(config.output_path);
    Summary summary;
    const std::uint64_t total = 21ULL * config.repetitions;
    for (const auto size : {payload_10_kib, payload_100_kib, payload_1_mib}) {
        const auto payload = make_payload(size);
        for (const auto id : algorithm_test_order) {
            const auto algorithm = create_signature_algorithm(id);
            for (std::uint64_t repetition = 1; repetition <= config.repetitions; ++repetition) {
                if (control.cancelled()) { summary.cancelled = true; return summary; }
                const auto completed = summary.succeeded + summary.failed;
                const std::string detail = std::string(algorithm->name()) + " / " +
                    std::to_string(size) + " bytes / repetição " + std::to_string(repetition);
                if (progress) progress({completed, total, detail});
                std::array<Measurement, 3> rows{};
                const std::array<Operation, 3> operations{Operation::keygen, Operation::sign, Operation::verify};
                for (std::size_t i = 0; i < rows.size(); ++i)
                    rows[i] = {static_cast<std::uint32_t>(repetition), config.client, Side::client,
                               id, size, operations[i], 0, false};
                bool success = false;
                try {
                    Connection connection(control);
                    connection.connect(address);
                    auto keys = measure_key_generation(*algorithm);
                    rows[0].duration_ns = keys.duration_ns;
                    rows[0].success = true;
                    check_cancelled(control);
                    auto signature = measure_signature(*algorithm, payload, keys.value.private_key);
                    rows[1].duration_ns = signature.duration_ns;
                    rows[1].success = true;
                    check_cancelled(control);
                    send_signed_payload(connection.get(), {payload, keys.value.public_key, signature.value}, *algorithm);
                    auto response = receive_signed_payload(connection.get(), size, *algorithm);
                    check_cancelled(control);
                    auto verified = measure_verification(*algorithm, response.payload, response.signature, response.public_key);
                    rows[2].duration_ns = verified.duration_ns;
                    rows[2].success = verified.value;
                    if (!verified.value) throw std::runtime_error("Assinatura da resposta inválida.");
                    success = true;
                } catch (const std::exception& error) {
                    summary.error = error.what();
                    summary.cancelled = control.cancelled();
                }
                for (const auto& row : rows) writer.write(row);
                writer.flush();
                if (success) ++summary.succeeded; else ++summary.failed;
                if (progress) progress({summary.succeeded + summary.failed, total, detail});
                if (!success) return summary;
            }
        }
    }
    summary.cancelled = control.cancelled();
    return summary;
}

}  // namespace tcc::client
