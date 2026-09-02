#pragma once

#include <cstddef>
#include <string_view>

#include "tcc/crypto/signature_algorithm.hpp"

namespace tcc::crypto::openssl {

class MlDsa final : public SignatureAlgorithm {
  public:
    explicit MlDsa(protocol::AlgorithmId algorithm_id);

    [[nodiscard]] protocol::AlgorithmId id() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] std::string_view provider() const noexcept override;

    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] SignatureResult sign(const Bytes& message,
                                       const Bytes& private_key) const override;
    [[nodiscard]] bool verify(const Bytes& message,
                              const Bytes& signature,
                              const Bytes& public_key) const override;

  private:
    protocol::AlgorithmId algorithm_id_;
    std::string_view algorithm_name_;
    std::size_t public_key_bytes_;
    std::size_t private_key_bytes_;
    std::size_t signature_bytes_;
};

}  // namespace tcc::crypto::openssl
