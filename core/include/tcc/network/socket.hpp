#pragma once

#include "tcc/network/endpoint.hpp"

namespace tcc::network {

class Socket {
public:
    using NativeHandle = int;
    static constexpr NativeHandle invalid_handle = -1;

    Socket() noexcept = default;
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    [[nodiscard]] static Socket adopt(NativeHandle handle);
    [[nodiscard]] static Socket connect_to(const Endpoint& endpoint);
    [[nodiscard]] static Socket listen_on(const Endpoint& endpoint,
                                          int backlog = 16);

    [[nodiscard]] Socket accept() const;
    void shutdown_write();
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] NativeHandle native_handle() const noexcept;

private:
    explicit Socket(NativeHandle handle) noexcept;

    NativeHandle handle_ = invalid_handle;
};

}  // namespace tcc::network
