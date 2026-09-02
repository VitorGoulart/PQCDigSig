#include <iostream>

#include "tcc/protocol/signed_content.hpp"
#include "test_support.hpp"

int main() {
    using namespace tcc::protocol;

    const auto content = make_signed_content(MessageType::signed_request,
                                             AlgorithmId::ed25519,
                                             42,
                                             9,
                                             {0xAA, 0xBB, 0xCC});
    const Bytes expected{
        0x03,
        0x07,
        0x00, 0x00, 0x00, 0x2A,
        0x00, 0x00, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x03,
        0xAA, 0xBB, 0xCC,
    };
    expect(content == expected,
           "canonical signed content does not match the specification");

    expect_protocol_error(
        [] {
            (void)make_signed_content(MessageType::start_run,
                                      AlgorithmId::ed25519,
                                      42,
                                      9,
                                      {});
        },
        ErrorCode::unexpected_message);

    expect_protocol_error(
        [] {
            (void)make_signed_content(MessageType::signed_response,
                                      AlgorithmId::invalid,
                                      42,
                                      9,
                                      {});
        },
        ErrorCode::invalid_algorithm);

    std::cout << "signed content tests passed\n";
}
