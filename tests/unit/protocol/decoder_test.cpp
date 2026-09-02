#include <iostream>
#include <variant>

#include "tcc/protocol/decoder.hpp"
#include "tcc/protocol/encoder.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "test_support.hpp"

int main() {
    using namespace tcc::protocol;

    const Message original = SignedResponse{55,
                                            999,
                                            {0x01, 0x02, 0x03},
                                            {0x10, 0x11},
                                            {0xFE, 0xFF}};
    const auto decoded = decode_message(encode_message(original));
    expect(std::holds_alternative<SignedResponse>(decoded),
           "decoded message has the wrong variant");
    const auto& response = std::get<SignedResponse>(decoded);
    expect(response.run_id == 55 && response.iteration == 999,
           "signed response identifiers changed during round-trip");
    expect(response.public_key == Bytes({0x01, 0x02, 0x03}) &&
               response.signature == Bytes({0x10, 0x11}) &&
               response.payload == Bytes({0xFE, 0xFF}),
           "signed response bytes changed during round-trip");

    const Message error = ErrorMessage{ErrorCode::invalid_signature,
                                       MessageType::signed_request,
                                       55,
                                       8,
                                       "assinatura inválida"};
    const auto decoded_error = decode_message(encode_message(error));
    const auto& error_value = std::get<ErrorMessage>(decoded_error);
    expect(error_value.error_code == ErrorCode::invalid_signature &&
               error_value.related_message_type == MessageType::signed_request &&
               error_value.run_id == 55 && error_value.iteration == 8 &&
               error_value.description == "assinatura inválida",
           "ERROR fields changed during round-trip");

    const Message start = StartRun{10,
                                   20,
                                   AlgorithmId::ml_dsa_65,
                                   limits::payload_100_kib,
                                   limits::iterations_per_run};
    const auto decoded_start = decode_message(encode_message(start));
    const auto& start_value = std::get<StartRun>(decoded_start);
    expect(start_value.experiment_id == 10 && start_value.run_id == 20 &&
               start_value.algorithm_id == AlgorithmId::ml_dsa_65 &&
               start_value.payload_length == limits::payload_100_kib,
           "START_RUN fields changed during round-trip");

    std::cout << "decoder tests passed\n";
}
