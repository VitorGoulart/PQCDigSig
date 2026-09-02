#pragma once

#include <cstddef>
#include <cstdint>

#include "tcc/network/socket.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::network {

void read_exact(const Socket& socket, std::uint8_t* output, std::size_t size);
void write_all(const Socket& socket,
               const std::uint8_t* data,
               std::size_t size);

[[nodiscard]] tcc::protocol::Bytes read_exact(const Socket& socket,
                                              std::size_t size);
void write_all(const Socket& socket, const tcc::protocol::Bytes& data);

[[nodiscard]] tcc::protocol::Message read_message(const Socket& socket);
void write_message(const Socket& socket,
                   const tcc::protocol::Message& message);

}  // namespace tcc::network
