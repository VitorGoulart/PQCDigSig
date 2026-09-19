#pragma once

#include "tcc/crypto/signature_algorithm.hpp"

#include <cstddef>
#include <memory>
#include <string_view>

typedef struct OQS_SIG OQS_SIG;

namespace tcc {

class MlDsa final : public SignatureAlgorithm {
public:
    explicit MlDsa(Algorithm algorithm);

    [[nodiscard]] Algorithm id() const noexcept override;
    [[nodiscard]] std::size_t public_key_size() const noexcept override;
    [[nodiscard]] std::size_t maximum_signature_size() const noexcept override;
    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] Bytes sign(
        const Bytes& payload,
        const Bytes& private_key) const override;
    [[nodiscard]] bool verify(
        const Bytes& payload,
        const Bytes& signature,
        const Bytes& public_key) const override;

private:
    Algorithm algorithm_;
    std::string_view openssl_name_;
    std::size_t public_key_bytes_;
    std::size_t private_key_bytes_;
    std::size_t signature_bytes_;
};

class RsaPss final : public SignatureAlgorithm {
public:
    [[nodiscard]] Algorithm id() const noexcept override;
    [[nodiscard]] std::size_t public_key_size() const noexcept override;
    [[nodiscard]] std::size_t maximum_signature_size() const noexcept override;
    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] Bytes sign(
        const Bytes& payload,
        const Bytes& private_key) const override;
    [[nodiscard]] bool verify(
        const Bytes& payload,
        const Bytes& signature,
        const Bytes& public_key) const override;
};

class Ed25519 final : public SignatureAlgorithm {
public:
    [[nodiscard]] Algorithm id() const noexcept override;
    [[nodiscard]] std::size_t public_key_size() const noexcept override;
    [[nodiscard]] std::size_t maximum_signature_size() const noexcept override;
    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] Bytes sign(
        const Bytes& payload,
        const Bytes& private_key) const override;
    [[nodiscard]] bool verify(
        const Bytes& payload,
        const Bytes& signature,
        const Bytes& public_key) const override;
};

class Falcon final : public SignatureAlgorithm {
public:
    explicit Falcon(Algorithm algorithm);
    ~Falcon() override;

    Falcon(const Falcon&) = delete;
    Falcon& operator=(const Falcon&) = delete;
    Falcon(Falcon&&) = delete;
    Falcon& operator=(Falcon&&) = delete;

    [[nodiscard]] Algorithm id() const noexcept override;
    [[nodiscard]] std::size_t public_key_size() const noexcept override;
    [[nodiscard]] std::size_t maximum_signature_size() const noexcept override;
    [[nodiscard]] KeyPair generate_key_pair() const override;
    [[nodiscard]] Bytes sign(
        const Bytes& payload,
        const Bytes& private_key) const override;
    [[nodiscard]] bool verify(
        const Bytes& payload,
        const Bytes& signature,
        const Bytes& public_key) const override;

private:
    struct Deleter {
        void operator()(OQS_SIG* value) const noexcept;
    };

    Algorithm algorithm_;
    std::unique_ptr<OQS_SIG, Deleter> implementation_;
};

}  // namespace tcc
