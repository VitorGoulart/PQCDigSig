#include <cstdint>
#include <iostream>

#include "tcc/protocol/encoder.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "test_support.hpp"

int main() {
    using namespace tcc::protocol;

    const Message start = StartRun{1,
                                   2,
                                   AlgorithmId::falcon_512,
                                   limits::payload_10_kib,
                                   limits::iterations_per_run};
    const Bytes expected_start{
        0x01, 0x00, 0x00, 0x00, 0x11,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x02,
        0x04,
        0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x03, 0xE8,
    };
    expect(encode_message(start) == expected_start,
           "START_RUN did not match the normative byte layout");

    const Message request = SignedRequest{2,
                                          7,
                                          {0xAA, 0xBB},
                                          {0xCC},
                                          {0x10, 0x20, 0x30}};
    const Bytes expected_request{
        0x03, 0x00, 0x00, 0x00, 0x1A,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x03,
        0xAA, 0xBB,
        0xCC,
        0x10, 0x20, 0x30,
    };
    expect(encode_message(request) == expected_request,
           "SIGNED_REQUEST did not match the normative byte layout");

    expect(encode_message(Message{Finish{}}) ==
               Bytes({0x07, 0x00, 0x00, 0x00, 0x00}),
           "FINISH must contain only an empty-body header");

    std::cout << "encoder tests passed\n";
}
