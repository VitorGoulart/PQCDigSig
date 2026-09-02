#include "tcc/protocol/messages.hpp"

#include <type_traits>

namespace tcc::protocol {

MessageType message_type(const Message& message) noexcept {
    return std::visit(
        [](const auto& value) noexcept {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, StartRun>) {
                return MessageType::start_run;
            } else if constexpr (std::is_same_v<T, StartRunAck>) {
                return MessageType::start_run_ack;
            } else if constexpr (std::is_same_v<T, SignedRequest>) {
                return MessageType::signed_request;
            } else if constexpr (std::is_same_v<T, SignedResponse>) {
                return MessageType::signed_response;
            } else if constexpr (std::is_same_v<T, EndRun>) {
                return MessageType::end_run;
            } else if constexpr (std::is_same_v<T, EndRunAck>) {
                return MessageType::end_run_ack;
            } else if constexpr (std::is_same_v<T, Finish>) {
                return MessageType::finish;
            } else {
                return MessageType::error;
            }
        },
        message);
}

}  // namespace tcc::protocol
