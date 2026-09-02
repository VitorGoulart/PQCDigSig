function(tcc_add_protocol_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE tcc::core)
    target_compile_features(${test_name} PRIVATE cxx_std_17)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

tcc_add_protocol_test(
    protocol_encoder_test
    unit/protocol/encoder_test.cpp
)
tcc_add_protocol_test(
    protocol_decoder_test
    unit/protocol/decoder_test.cpp
)
tcc_add_protocol_test(
    protocol_malformed_message_test
    unit/protocol/malformed_message_test.cpp
)
tcc_add_protocol_test(
    protocol_signed_content_test
    unit/protocol/signed_content_test.cpp
)
tcc_add_protocol_test(
    protocol_state_machine_test
    unit/protocol/state_machine_test.cpp
)

find_package(Threads REQUIRED)
tcc_add_protocol_test(
    network_socket_io_test
    unit/network/socket_io_test.cpp
)
target_link_libraries(network_socket_io_test PRIVATE Threads::Threads)

tcc_add_protocol_test(
    crypto_signature_algorithm_test
    unit/crypto/signature_algorithm_test.cpp
)
