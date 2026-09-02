#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "tcc/protocol/algorithm_id.hpp"
#include "tcc/protocol/messages.hpp"

namespace tcc::protocol {

enum class EndpointRole {
    client,
    server,
};

enum class SessionState {
    ready,
    waiting_start_run_ack,
    waiting_signed_request,
    waiting_signed_response,
    waiting_end_run,
    waiting_end_run_ack,
    finished,
    failed,
};

struct ActiveRun {
    std::uint32_t experiment_id;
    std::uint32_t run_id;
    AlgorithmId algorithm_id;
    std::uint32_t payload_length;
    std::uint32_t iteration_count;
};

class SessionStateMachine {
public:
    explicit SessionStateMachine(EndpointRole role) noexcept;

    void on_send(const Message& message);
    void on_receive(const Message& message);
    void abort() noexcept;

    [[nodiscard]] EndpointRole role() const noexcept;
    [[nodiscard]] SessionState state() const noexcept;
    [[nodiscard]] const std::optional<ActiveRun>& active_run() const noexcept;
    [[nodiscard]] std::uint32_t next_iteration() const noexcept;
    [[nodiscard]] std::size_t started_run_count() const noexcept;

private:
    enum class Actor {
        client,
        server,
    };

    void advance(const Message& message, Actor actor);
    void accept_start_run(const StartRun& message);
    void accept_start_run_ack(const StartRunAck& message);
    void accept_signed_request(const SignedRequest& message);
    void accept_signed_response(const SignedResponse& message);
    void accept_end_run(const EndRun& message);
    void accept_end_run_ack(const EndRunAck& message);

    void require_active_run(std::uint32_t run_id) const;
    void require_payload_size(std::size_t payload_size) const;

    EndpointRole role_;
    SessionState state_ = SessionState::ready;
    std::optional<ActiveRun> active_run_;
    std::uint32_t next_iteration_ = 0;
    std::vector<std::uint32_t> used_run_ids_;
};

}  // namespace tcc::protocol
