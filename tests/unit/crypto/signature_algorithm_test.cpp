#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "tcc/crypto/algorithm_factory.hpp"
#include "tcc/crypto/crypto_error.hpp"
#include "tcc/crypto/signature_algorithm.hpp"
#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "../protocol/test_support.hpp"

namespace {

using tcc::crypto::Bytes;
using tcc::crypto::SignatureAlgorithm;
using tcc::protocol::AlgorithmId;

void exercise_algorithm(const AlgorithmId id,
                        const std::string& expected_name,
                        const std::string& expected_provider) {
    std::unique_ptr<SignatureAlgorithm> algorithm =
        tcc::crypto::create_signature_algorithm(id);
    expect(algorithm->id() == id, "factory returned an algorithm with wrong ID");
    expect(algorithm->name() == expected_name, "unexpected algorithm name");
    expect(algorithm->provider() == expected_provider,
           "unexpected provider name");

    const auto key_pair = algorithm->generate_key_pair();
    expect(!key_pair.public_key.empty(), "public key must not be empty");
    expect(!key_pair.private_key.empty(), "private key must not be empty");
    expect(key_pair.public_key.size() <=
               tcc::protocol::limits::max_public_key_bytes,
           "public key exceeds protocol limit");

    const Bytes message{'c', 'o', 'm', 'm', 'o', 'n', '-', 'a', 'p', 'i'};
    const auto signed_message = algorithm->sign(message, key_pair.private_key);
    expect(!signed_message.signature.empty(), "signature must not be empty");
    expect(signed_message.signature.size() <=
               tcc::protocol::limits::max_signature_bytes,
           "signature exceeds protocol limit");
    expect(algorithm->verify(message, signed_message.signature,
                             key_pair.public_key),
           "valid signature was rejected");

    Bytes tampered_message = message;
    tampered_message.front() ^= 0x01U;
    expect(!algorithm->verify(tampered_message, signed_message.signature,
                              key_pair.public_key),
           "tampered message was accepted");

    Bytes tampered_signature = signed_message.signature;
    tampered_signature.back() ^= 0x01U;
    expect(!algorithm->verify(message, tampered_signature, key_pair.public_key),
           "tampered signature was accepted");

    Bytes invalid_private_key = key_pair.private_key;
    invalid_private_key.pop_back();
    expect_crypto_error(
        [&] { static_cast<void>(algorithm->sign(message, invalid_private_key)); },
        tcc::crypto::CryptoErrorCode::invalid_key);

    Bytes invalid_public_key = key_pair.public_key;
    invalid_public_key.pop_back();
    expect_crypto_error(
        [&] {
            static_cast<void>(algorithm->verify(
                message, signed_message.signature, invalid_public_key));
        },
        tcc::crypto::CryptoErrorCode::invalid_key);
}

void test_factory_availability_and_invalid_ids() {
    const AlgorithmId implemented_ids[] = {
        AlgorithmId::ml_dsa_44, AlgorithmId::ml_dsa_65,
        AlgorithmId::ml_dsa_87, AlgorithmId::falcon_512,
        AlgorithmId::falcon_1024, AlgorithmId::rsa_pss,
        AlgorithmId::ed25519,
    };
    for (const AlgorithmId id : implemented_ids) {
        expect(tcc::crypto::is_signature_algorithm_implemented(id),
               "known algorithm should be reported as implemented");
    }

    expect(!tcc::crypto::is_signature_algorithm_implemented(
               AlgorithmId::invalid),
           "invalid ID must not be reported as implemented");
    expect(!tcc::crypto::is_signature_algorithm_implemented(
               static_cast<AlgorithmId>(0xFFU)),
           "unknown ID must not be reported as implemented");
    expect_crypto_error(
        [] {
            static_cast<void>(tcc::crypto::create_signature_algorithm(
                AlgorithmId::invalid));
        },
        tcc::crypto::CryptoErrorCode::unsupported_algorithm);
    expect_crypto_error(
        [] {
            static_cast<void>(tcc::crypto::create_signature_algorithm(
                static_cast<AlgorithmId>(0xFFU)));
        },
        tcc::crypto::CryptoErrorCode::unsupported_algorithm);
}

}  // namespace

int main() {
    try {
        exercise_algorithm(AlgorithmId::ml_dsa_44, "ML-DSA-44", "OpenSSL");
        exercise_algorithm(AlgorithmId::ml_dsa_65, "ML-DSA-65", "OpenSSL");
        exercise_algorithm(AlgorithmId::ml_dsa_87, "ML-DSA-87", "OpenSSL");
        exercise_algorithm(AlgorithmId::falcon_512, "Falcon-512", "liboqs");
        exercise_algorithm(AlgorithmId::falcon_1024, "Falcon-1024", "liboqs");
        exercise_algorithm(AlgorithmId::rsa_pss, "RSASSA-PSS", "OpenSSL");
        exercise_algorithm(AlgorithmId::ed25519, "Ed25519", "OpenSSL");
        test_factory_availability_and_invalid_ids();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
