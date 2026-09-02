#include "tcc/protocol/state_machine.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "protocol_validation.hpp"
#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"

namespace tcc::protocol {
namespace {

[[noreturn]] void unexpected(const char* message) {
    throw ProtocolError(ErrorCode::unexpected_message, message);
}

[[nodiscard]] SessionState expected_state(const MessageType type) {
    switch (type) {
        case MessageType::start_run:
        case MessageType::finish:
            return SessionState::ready;
        case MessageType::start_run_ack:
            return SessionState::waiting_start_run_ack;
        case MessageType::signed_request:
            return SessionState::waiting_signed_request;
        case MessageType::signed_response:
            return SessionState::waiting_signed_response;
        case MessageType::end_run:
            return SessionState::waiting_end_run;
        case MessageType::end_run_ack:
            return SessionState::waiting_end_run_ack;
        case MessageType::error:
            break;
    }
    unexpected("ERROR is accepted from any active protocol state");
}

}  // namespace

SessionStateMachine::SessionStateMachine(const EndpointRole role) noexcept
    : role_(role) {}

void SessionStateMachine::on_send(const Message& message) {
    advance(message,
            role_ == EndpointRole::client ? Actor::client : Actor::server);
}

void SessionStateMachine::on_receive(const Message& message) {
    advance(message,
            role_ == EndpointRole::client ? Actor::server : Actor::client);
}

void SessionStateMachine::abort() noexcept {
    if (state_ == SessionState::finished) {
        return;
    }
    state_ = SessionState::failed;
    active_run_.reset();
    next_iteration_ = 0;
}

EndpointRole SessionStateMachine::role() const noexcept { return role_; }

SessionState SessionStateMachine::state() const noexcept { return state_; }

const std::optional<ActiveRun>& SessionStateMachine::active_run() const noexcept {
    return active_run_;
}

std::uint32_t SessionStateMachine::next_iteration() const noexcept {
    return next_iteration_;
}

std::size_t SessionStateMachine::started_run_count() const noexcept {
    return used_run_ids_.size();
}

void SessionStateMachine::advance(const Message& message, const Actor actor) {
    if (state_ == SessionState::finished) {
        unexpected("the session is already finished");
    }
    if (state_ == SessionState::failed) {
        unexpected("the session is in a terminal failure state");
    }

    detail::validate_message(message);
    const auto type = message_type(message);
    if (type == MessageType::error) {
        abort();
        return;
    }

    const bool client_message =
        type == MessageType::start_run ||
        type == MessageType::signed_request || type == MessageType::end_run ||
        type == MessageType::finish;
    const auto expected_actor =
        client_message ? Actor::client : Actor::server;
    if (actor != expected_actor) {
        unexpected("message was sent in the wrong direction");
    }
    if (state_ != expected_state(type)) {
        unexpected("message is not valid in the current protocol state");
    }

    switch (type) {
        case MessageType::start_run:
            accept_start_run(std::get<StartRun>(message));
            return;
        case MessageType::start_run_ack:
            accept_start_run_ack(std::get<StartRunAck>(message));
            return;
        case MessageType::signed_request:
            accept_signed_request(std::get<SignedRequest>(message));
            return;
        case MessageType::signed_response:
            accept_signed_response(std::get<SignedResponse>(message));
            return;
        case MessageType::end_run:
            accept_end_run(std::get<EndRun>(message));
            return;
        case MessageType::end_run_ack:
            accept_end_run_ack(std::get<EndRunAck>(message));
            return;
        case MessageType::finish:
            state_ = SessionState::finished;
            return;
        case MessageType::error:
            return;
    }
}

void SessionStateMachine::accept_start_run(const StartRun& message) {
    if (used_run_ids_.size() >= limits::max_runs_per_connection) {
        throw ProtocolError(ErrorCode::limit_exceeded,
                            "connection exceeded its maximum number of runs");
    }
    if (std::find(used_run_ids_.begin(), used_run_ids_.end(), message.run_id) !=
        used_run_ids_.end()) {
        throw ProtocolError(ErrorCode::invalid_identifier,
                            "run_id was already used in this connection");
    }

    active_run_ = ActiveRun{message.experiment_id,
                            message.run_id,
                            message.algorithm_id,
                            message.payload_length,
                            message.iteration_count};
    used_run_ids_.push_back(message.run_id);
    next_iteration_ = 0;
    state_ = SessionState::waiting_start_run_ack;
}

void SessionStateMachine::accept_start_run_ack(const StartRunAck& message) {
    require_active_run(message.run_id);
    if (message.status == StartRunStatus::accepted) {
        state_ = SessionState::waiting_signed_request;
        return;
    }

    active_run_.reset();
    next_iteration_ = 0;
    state_ = SessionState::ready;
}

void SessionStateMachine::accept_signed_request(
    const SignedRequest& message) {
    require_active_run(message.run_id);
    if (message.iteration != next_iteration_) {
        throw ProtocolError(ErrorCode::invalid_body,
                            "SIGNED_REQUEST iteration is not the next expected iteration");
    }
    require_payload_size(message.payload.size());
    state_ = SessionState::waiting_signed_response;
}

void SessionStateMachine::accept_signed_response(
    const SignedResponse& message) {
    require_active_run(message.run_id);
    if (message.iteration != next_iteration_) {
        throw ProtocolError(ErrorCode::invalid_body,
                            "SIGNED_RESPONSE iteration does not match its request");
    }
    require_payload_size(message.payload.size());

    ++next_iteration_;
    state_ = next_iteration_ == active_run_->iteration_count
                 ? SessionState::waiting_end_run
                 : SessionState::waiting_signed_request;
}

void SessionStateMachine::accept_end_run(const EndRun& message) {
    require_active_run(message.run_id);
    if (message.completed_iterations != next_iteration_) {
        throw ProtocolError(
            ErrorCode::invalid_body,
            "END_RUN completed_iterations does not match processed iterations");
    }
    state_ = SessionState::waiting_end_run_ack;
}

void SessionStateMachine::accept_end_run_ack(const EndRunAck& message) {
    require_active_run(message.run_id);
    active_run_.reset();
    next_iteration_ = 0;
    state_ = SessionState::ready;
}

void SessionStateMachine::require_active_run(
    const std::uint32_t run_id) const {
    if (!active_run_.has_value() || active_run_->run_id != run_id) {
        throw ProtocolError(ErrorCode::invalid_identifier,
                            "message run_id does not match the active run");
    }
}

void SessionStateMachine::require_payload_size(
    const std::size_t payload_size) const {
    if (!active_run_.has_value() ||
        payload_size != active_run_->payload_length) {
        throw ProtocolError(ErrorCode::invalid_body,
                            "payload size does not match START_RUN");
    }
}

}  // namespace tcc::protocol
