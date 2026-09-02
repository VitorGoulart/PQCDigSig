add_library(
    tcc_core
    STATIC
    src/benchmark/experiment_runner.cpp
    src/benchmark/payload_generator.cpp
    src/benchmark/timer.cpp
    src/crypto/algorithm_factory.cpp
    src/crypto/crypto_error.cpp
    src/crypto/openssl/ed25519.cpp
    src/crypto/openssl/ml_dsa.cpp
    src/crypto/openssl/openssl_helpers.cpp
    src/crypto/openssl/rsa_pss.cpp
    src/crypto/oqs/oqs_helpers.cpp
    src/crypto/oqs/oqs_signature.cpp
    src/network/network_error.cpp
    src/network/socket.cpp
    src/network/socket_io.cpp
    src/protocol/decoder.cpp
    src/protocol/encoder.cpp
    src/protocol/messages.cpp
    src/protocol/protocol_error.cpp
    src/protocol/protocol_validation.cpp
    src/protocol/signed_content.cpp
    src/protocol/state_machine.cpp
    src/results/csv_writer.cpp
    src/results/metadata.cpp
)

add_library(tcc::core ALIAS tcc_core)

target_include_directories(
    tcc_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(tcc_core PUBLIC cxx_std_17)
target_link_libraries(tcc_core PUBLIC OQS::oqs OpenSSL::Crypto)

set_target_properties(
    tcc_core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)
