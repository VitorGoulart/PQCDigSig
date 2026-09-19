#pragma once

#include <stdexcept>
#include <string>

namespace tcc {

enum class CryptoErrorCode {
    invalid_key,
    unsupported_algorithm,
    provider_unavailable,
    key_generation_failed,
    signing_failed,
    verification_failed,
};

class CryptoError : public std::runtime_error {
public:
    CryptoError(CryptoErrorCode code, std::string message);

    [[nodiscard]] CryptoErrorCode code() const noexcept;

private:
    CryptoErrorCode code_;
};

}  // namespace tcc
