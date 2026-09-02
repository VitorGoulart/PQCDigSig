#pragma once

#include <memory>

#include <oqs/oqs.h>

#include "tcc/crypto/signature_algorithm.hpp"

namespace tcc::crypto::oqs {

class OqsSignature final : public SignatureAlgorithm {
  public:
    explicit OqsSignature(protocol::AlgorithmId algorithm_id);

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
    struct Deleter {
        void operator()(OQS_SIG* value) const noexcept;
    };

    protocol::AlgorithmId algorithm_id_;
    std::unique_ptr<OQS_SIG, Deleter> implementation_;
};

}  // namespace tcc::crypto::oqs
