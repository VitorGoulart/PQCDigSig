#pragma once

#include "tcc/crypto.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace tcc {

inline constexpr std::uint16_t tcp_port = 62037U;

// Ordem de transmissão: payload, public_key e signature.
struct SignedPayload {
    Bytes payload;
    Bytes public_key;
    Bytes signature;
};

class TransportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Envia os três campos e encerra somente a direção de escrita do socket.
void send_signed_payload(
    int socket_fd,
    const SignedPayload& message,
    const SignatureAlgorithm& algorithm);

// Lê carga e chave nos tamanhos conhecidos e a assinatura até EOF.
[[nodiscard]] SignedPayload receive_signed_payload(
    int socket_fd,
    std::size_t payload_size,
    const SignatureAlgorithm& algorithm);

}  // namespace tcc
