#include <cstdint>
#include <iostream>
#include <variant>

#include "tcc/protocol/protocol_limits.hpp"
#include "tcc/protocol/state_machine.hpp"
#include "test_support.hpp"

namespace {

using namespace tcc::protocol;

Message make_start(const std::uint32_t run_id) {
    return StartRun{1,
                    run_id,
                    AlgorithmId::ed25519,
                    limits::payload_10_kib,
                    limits::iterations_per_run};
}

void test_complete_shared_run() {
    SessionStateMachine client(EndpointRole::client);
    SessionStateMachine server(EndpointRole::server);

    const auto start = make_start(7);
    client.on_send(start);
    server.on_receive(start);
    expect(client.state() == SessionState::waiting_start_run_ack &&
               server.state() == SessionState::waiting_start_run_ack,
           "START_RUN did not advance to its acknowledgement");
    expect(client.active_run().has_value() &&
               client.active_run()->run_id == 7 &&
               client.started_run_count() == 1,
           "active run was not recorded");

    const Message start_ack = StartRunAck{7, StartRunStatus::accepted};
    server.on_send(start_ack);
    client.on_receive(start_ack);
    expect(client.state() == SessionState::waiting_signed_request &&
               server.state() == SessionState::waiting_signed_request,
           "accepted START_RUN_ACK did not begin the iteration loop");

    Message request = SignedRequest{7,
                                    0,
                                    {0x01},
                                    {0x02},
                                    Bytes(limits::payload_10_kib, 0xA5)};
    Message response = SignedResponse{7,
                                      0,
                                      {0x03},
                                      {0x04},
                                      Bytes(limits::payload_10_kib, 0x5A)};
    for (std::uint32_t iteration = 0;
         iteration < limits::iterations_per_run;
         ++iteration) {
        std::get<SignedRequest>(request).iteration = iteration;
        std::get<SignedResponse>(response).iteration = iteration;
        client.on_send(request);
        server.on_receive(request);
        expect(client.state() == SessionState::waiting_signed_response &&
                   server.state() == SessionState::waiting_signed_response,
               "SIGNED_REQUEST did not advance to SIGNED_RESPONSE");
        server.on_send(response);
        client.on_receive(response);
    }

    expect(client.next_iteration() == limits::iterations_per_run &&
               server.next_iteration() == limits::iterations_per_run &&
               client.state() == SessionState::waiting_end_run &&
               server.state() == SessionState::waiting_end_run,
           "the iteration loop did not require END_RUN after 1000 responses");
    const Message end = EndRun{7, limits::iterations_per_run};
    client.on_send(end);
    server.on_receive(end);
    const Message end_ack = EndRunAck{7, EndRunStatus::completed};
    server.on_send(end_ack);
    client.on_receive(end_ack);
    expect(client.state() == SessionState::ready &&
               server.state() == SessionState::ready &&
               !client.active_run().has_value() &&
               !server.active_run().has_value() &&
               client.next_iteration() == 0 && server.next_iteration() == 0,
           "END_RUN_ACK did not return the session to ready");

    const Message finish = Finish{};
    client.on_send(finish);
    server.on_receive(finish);
    expect(client.state() == SessionState::finished &&
               server.state() == SessionState::finished,
           "FINISH did not terminate the session");
}

void test_server_direction() {
    SessionStateMachine machine(EndpointRole::server);
    machine.on_receive(make_start(11));
    machine.on_send(Message{StartRunAck{11, StartRunStatus::accepted}});
    expect(machine.state() == SessionState::waiting_signed_request,
           "server did not accept the client/server message directions");

    expect_protocol_error(
        [&machine] {
            machine.on_send(Message{SignedRequest{
                11, 0, {0x01}, {0x02}, Bytes(limits::payload_10_kib)}});
        },
        ErrorCode::unexpected_message);
    expect(machine.state() == SessionState::waiting_signed_request,
           "wrong-direction message changed the state");
}

void test_context_validation() {
    SessionStateMachine machine(EndpointRole::client);
    machine.on_send(make_start(21));

    expect_protocol_error(
        [&machine] {
            machine.on_receive(
                Message{StartRunAck{22, StartRunStatus::accepted}});
        },
        ErrorCode::invalid_identifier);
    expect(machine.state() == SessionState::waiting_start_run_ack,
           "wrong run_id changed the state");

    machine.on_receive(Message{StartRunAck{21, StartRunStatus::accepted}});
    expect_protocol_error(
        [&machine] {
            machine.on_send(Message{SignedRequest{
                21, 1, {0x01}, {0x02}, Bytes(limits::payload_10_kib)}});
        },
        ErrorCode::invalid_body);
    expect(machine.state() == SessionState::waiting_signed_request,
           "out-of-order iteration changed the state");

    expect_protocol_error(
        [&machine] {
            machine.on_send(Message{
                SignedRequest{21, 0, {0x01}, {0x02}, Bytes(9)}});
        },
        ErrorCode::invalid_body);
    expect(machine.state() == SessionState::waiting_signed_request,
           "wrong payload size changed the state");

    machine.on_send(Message{ErrorMessage{ErrorCode::cryptographic_error,
                                         MessageType::signed_request,
                                         21,
                                         0,
                                         "falha local"}});
    expect(machine.state() == SessionState::failed &&
               !machine.active_run().has_value(),
           "ERROR did not abort the session");
    expect_protocol_error(
        [&machine] { machine.on_send(Message{Finish{}}); },
        ErrorCode::unexpected_message);
}

void test_reused_id_and_run_limit() {
    SessionStateMachine reused(EndpointRole::client);
    reused.on_send(make_start(1));
    reused.on_receive(
        Message{StartRunAck{1, StartRunStatus::server_busy}});
    expect(reused.state() == SessionState::ready,
           "rejected run did not return to ready");
    expect_protocol_error(
        [&reused] { reused.on_send(make_start(1)); },
        ErrorCode::invalid_identifier);

    SessionStateMachine limited(EndpointRole::client);
    for (std::uint32_t run_id = 1;
         run_id <= limits::max_runs_per_connection;
         ++run_id) {
        limited.on_send(make_start(run_id));
        limited.on_receive(
            Message{StartRunAck{run_id, StartRunStatus::server_busy}});
    }
    expect_protocol_error(
        [&limited] {
            limited.on_send(make_start(limits::max_runs_per_connection + 1));
        },
        ErrorCode::limit_exceeded);
}

}  // namespace

int main() {
    test_complete_shared_run();
    test_server_direction();
    test_context_validation();
    test_reused_id_and_run_limit();
    std::cout << "state machine tests passed\n";
}
