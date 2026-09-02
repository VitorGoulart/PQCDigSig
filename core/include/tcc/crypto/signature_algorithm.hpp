#pragma once

#include <string_view>

#include "tcc/crypto/key_pair.hpp"
#include "tcc/crypto/signature_result.hpp"
#include "tcc/protocol/algorithm_id.hpp"

namespace tcc::crypto {

class SignatureAlgorithm {
  public:
    virtual ~SignatureAlgorithm() = default;

    [[nodiscard]] virtual protocol::AlgorithmId id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::string_view provider() const noexcept = 0;

    [[nodiscard]] virtual KeyPair generate_key_pair() const = 0;
    [[nodiscard]] virtual SignatureResult sign(
        const Bytes& message,
        const Bytes& private_key) const = 0;
    [[nodiscard]] virtual bool verify(const Bytes& message,
                                      const Bytes& signature,
                                      const Bytes& public_key) const = 0;
};

}  // namespace tcc::crypto
