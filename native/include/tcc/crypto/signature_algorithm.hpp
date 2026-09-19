#pragma once

#include "tcc/crypto/algorithm.hpp"
#include "tcc/crypto/key_pair.hpp"

#include <cstddef>
#include <string_view>

namespace tcc {

class SignatureAlgorithm {
public:
    virtual ~SignatureAlgorithm() = default;

    [[nodiscard]] virtual Algorithm id() const noexcept = 0;
    [[nodiscard]] virtual std::size_t public_key_size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t maximum_signature_size() const noexcept =
        0;

    [[nodiscard]] std::string_view name() const noexcept {
        return algorithm_name(id());
    }

    [[nodiscard]] CryptoProvider provider() const {
        return algorithm_provider(id());
    }

    [[nodiscard]] virtual KeyPair generate_key_pair() const = 0;
    [[nodiscard]] virtual Bytes sign(
        const Bytes& payload,
        const Bytes& private_key) const = 0;
    [[nodiscard]] virtual bool verify(
        const Bytes& payload,
        const Bytes& signature,
        const Bytes& public_key) const = 0;
};

}  // namespace tcc
