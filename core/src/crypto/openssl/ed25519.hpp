#pragma once

#include "tcc/crypto/signature_algorithm.hpp"

namespace tcc::crypto::openssl {

class Ed25519 final : public SignatureAlgorithm {
  public:
    [[nodiscard]] protocol::AlgorithmId id() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] std::string_view provider() const noexcept override;

    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] SignatureResult sign(const Bytes& message,
                                       const Bytes& private_key) const override;
    [[nodiscard]] bool verify(const Bytes& message,
                              const Bytes& signature,
                              const Bytes& public_key) const override;
};

}  // namespace tcc::crypto::openssl
