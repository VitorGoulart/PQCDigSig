#include "tcc/crypto.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Callable>
void expect_crypto_error(
    Callable&& callable,
    const tcc::CryptoErrorCode expected_code) {
    try {
        callable();
        throw std::runtime_error("expected tcc::CryptoError");
    } catch (const tcc::CryptoError& error) {
        expect(error.code() == expected_code, "unexpected CryptoError code");
    }
}

void exercise_algorithm(const tcc::Algorithm id) {
    std::unique_ptr<tcc::SignatureAlgorithm> algorithm =
        tcc::create_signature_algorithm(id);

    expect(algorithm->id() == id, "factory returned the wrong algorithm");
    expect(
        algorithm->name() == tcc::algorithm_name(id),
        "algorithm returned the wrong name");
    expect(
        algorithm->provider() == tcc::algorithm_provider(id),
        "algorithm returned the wrong provider");

    const tcc::KeyPair key_pair = algorithm->generate_key_pair();
    expect(!key_pair.public_key.empty(), "public key is empty");
    expect(!key_pair.private_key.empty(), "private key is empty");
    expect(
        key_pair.public_key.size() == algorithm->public_key_size(),
        "public key has an unexpected size");

    const tcc::Bytes payload{'a', 'd', 'a', 'p', 't', 'e', 'r'};
    const tcc::Bytes signature =
        algorithm->sign(payload, key_pair.private_key);
    expect(!signature.empty(), "signature is empty");
    expect(
        signature.size() <= algorithm->maximum_signature_size(),
        "signature exceeds the advertised maximum size");
    expect(
        algorithm->verify(payload, signature, key_pair.public_key),
        "valid signature was rejected");

    tcc::Bytes altered_payload = payload;
    altered_payload.front() ^= 0x01U;
    expect(
        !algorithm->verify(
            altered_payload, signature, key_pair.public_key),
        "altered payload was accepted");

    tcc::Bytes altered_signature = signature;
    altered_signature.back() ^= 0x01U;
    expect(
        !algorithm->verify(payload, altered_signature, key_pair.public_key),
        "altered signature was accepted");

    tcc::Bytes invalid_private_key = key_pair.private_key;
    invalid_private_key.pop_back();
    expect_crypto_error(
        [&] {
            static_cast<void>(algorithm->sign(payload, invalid_private_key));
        },
        tcc::CryptoErrorCode::invalid_key);

    tcc::Bytes invalid_public_key = key_pair.public_key;
    invalid_public_key.pop_back();
    expect_crypto_error(
        [&] {
            static_cast<void>(algorithm->verify(
                payload, signature, invalid_public_key));
        },
        tcc::CryptoErrorCode::invalid_key);
}

}  // namespace

int main() {
    try {
        for (const tcc::Algorithm algorithm : tcc::algorithm_test_order) {
            expect(
                tcc::is_supported_algorithm(algorithm),
                "known algorithm was reported as unsupported");
            exercise_algorithm(algorithm);
        }

        const auto invalid = static_cast<tcc::Algorithm>(0xFFU);
        expect(
            !tcc::is_supported_algorithm(invalid),
            "unknown algorithm was reported as supported");
        expect_crypto_error(
            [&] {
                static_cast<void>(tcc::create_signature_algorithm(invalid));
            },
            tcc::CryptoErrorCode::unsupported_algorithm);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
