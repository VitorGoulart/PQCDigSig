#include <iostream>

#include "tcc/protocol/decoder.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "test_support.hpp"

int main() {
    using namespace tcc::protocol;

    expect_protocol_error(
        [] { (void)decode_header(Bytes{0x00, 0x00, 0x00, 0x00, 0x00}); },
        ErrorCode::unexpected_message);

    expect_protocol_error(
        [] { (void)decode_header(Bytes{0x01, 0x00, 0x20, 0x00, 0x01}); },
        ErrorCode::limit_exceeded);

    expect_protocol_error(
        [] { (void)decode_message(Bytes{0x07, 0x00, 0x00, 0x00, 0x01}); },
        ErrorCode::invalid_body);

    const Bytes invalid_algorithm_start{
        0x01, 0x00, 0x00, 0x00, 0x11,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01,
        0x00,
        0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x03, 0xE8,
    };
    expect_protocol_error(
        [&invalid_algorithm_start] {
            (void)decode_message(invalid_algorithm_start);
        },
        ErrorCode::invalid_algorithm);

    const Bytes inconsistent_signed_lengths{
        0x03, 0x00, 0x00, 0x00, 0x15,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xAA,
    };
    expect_protocol_error(
        [&inconsistent_signed_lengths] {
            (void)decode_message(inconsistent_signed_lengths);
        },
        ErrorCode::invalid_body);

    const Bytes invalid_utf8_error{
        0x08, 0x00, 0x00, 0x00, 0x0F,
        0x00, 0x02,
        0x03,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x02,
        0xC0, 0xAF,
    };
    expect_protocol_error(
        [&invalid_utf8_error] { (void)decode_message(invalid_utf8_error); },
        ErrorCode::invalid_body);

    std::cout << "malformed message tests passed\n";
}
