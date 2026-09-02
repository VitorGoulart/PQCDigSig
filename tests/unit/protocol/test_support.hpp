#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include "tcc/crypto/crypto_error.hpp"
#include "tcc/network/network_error.hpp"
#include "tcc/protocol/protocol_error.hpp"

inline void expect(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void expect_protocol_error(Function&& function,
                           const tcc::protocol::ErrorCode expected_code) {
    try {
        std::invoke(std::forward<Function>(function));
    } catch (const tcc::protocol::ProtocolError& error) {
        expect(error.code() == expected_code, "unexpected protocol error code");
        return;
    }
    throw std::runtime_error("expected ProtocolError was not thrown");
}

template <typename Function>
void expect_network_error(Function&& function,
                          const tcc::network::NetworkErrorCode expected_code) {
    try {
        std::invoke(std::forward<Function>(function));
    } catch (const tcc::network::NetworkError& error) {
        expect(error.code() == expected_code, "unexpected network error code");
        return;
    }
    throw std::runtime_error("expected NetworkError was not thrown");
}

template <typename Function>
void expect_crypto_error(Function&& function,
                         const tcc::crypto::CryptoErrorCode expected_code) {
    try {
        std::invoke(std::forward<Function>(function));
    } catch (const tcc::crypto::CryptoError& error) {
        expect(error.code() == expected_code, "unexpected crypto error code");
        return;
    }
    throw std::runtime_error("expected CryptoError was not thrown");
}
