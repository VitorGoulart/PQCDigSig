#pragma once

#include "tcc/crypto/error.hpp"

#include <memory>
#include <string_view>

#include <openssl/evp.h>

namespace tcc::openssl_detail {

struct EvpPkeyDeleter {
    void operator()(EVP_PKEY* value) const noexcept;
};

struct EvpPkeyContextDeleter {
    void operator()(EVP_PKEY_CTX* value) const noexcept;
};

struct EvpMessageDigestContextDeleter {
    void operator()(EVP_MD_CTX* value) const noexcept;
};

using EvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using EvpPkeyContext =
    std::unique_ptr<EVP_PKEY_CTX, EvpPkeyContextDeleter>;
using EvpMessageDigestContext =
    std::unique_ptr<EVP_MD_CTX, EvpMessageDigestContextDeleter>;

void clear_error_queue() noexcept;

[[noreturn]] void throw_openssl_error(
    CryptoErrorCode code,
    std::string_view operation);

}  // namespace tcc::openssl_detail
