#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <thread>
#include <utility>

#include <netinet/in.h>
#include <sys/socket.h>

#include "tcc/network/socket.hpp"
#include "tcc/network/socket_io.hpp"
#include "tcc/protocol/protocol_error.hpp"
#include "tcc/protocol/protocol_limits.hpp"
#include "tcc/protocol/state_machine.hpp"
#include "../protocol/test_support.hpp"

namespace {

using namespace tcc::network;
using namespace tcc::protocol;

std::pair<Socket, Socket> make_socket_pair() {
    std::array<int, 2> handles{};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, handles.data()) != 0) {
        throw std::runtime_error("could not create socket pair for test");
    }
    return {Socket::adopt(handles[0]), Socket::adopt(handles[1])};
}

std::uint16_t local_port(const Socket& listener) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(listener.native_handle(),
                      reinterpret_cast<sockaddr*>(&address),
                      &length) != 0 ||
        address.sin_family != AF_INET) {
        throw std::runtime_error("could not inspect loopback listener");
    }
    return ntohs(address.sin_port);
}

void test_large_exact_transfer() {
    auto sockets = make_socket_pair();
    auto& writer = sockets.first;
    auto& reader = sockets.second;
    const Bytes expected(4U * 1024U * 1024U, 0xA5);

    std::exception_ptr writer_error;
    std::thread writer_thread([&] {
        try {
            write_all(writer, expected);
            writer.shutdown_write();
        } catch (...) {
            writer_error = std::current_exception();
            writer.close();
        }
    });

    Bytes received;
    std::exception_ptr reader_error;
    try {
        received = read_exact(reader, expected.size());
    } catch (...) {
        reader_error = std::current_exception();
    }
    writer_thread.join();

    if (writer_error) {
        std::rethrow_exception(writer_error);
    }
    if (reader_error) {
        std::rethrow_exception(reader_error);
    }
    expect(received == expected,
           "read_exact/write_all changed a partially transferred stream");
}

void test_disconnect_and_header_limit() {
    {
        auto sockets = make_socket_pair();
        const Bytes incomplete_frame{
            0x03, 0x00, 0x00, 0x00, 0x14,
        };
        write_all(sockets.first, incomplete_frame);
        sockets.first.close();
        expect_network_error(
            [&sockets] { (void)read_message(sockets.second); },
            NetworkErrorCode::peer_closed);
    }

    {
        auto sockets = make_socket_pair();
        const Bytes oversized_header{
            0x03, 0x00, 0x20, 0x00, 0x01,
        };
        write_all(sockets.first, oversized_header);
        expect_protocol_error(
            [&sockets] { (void)read_message(sockets.second); },
            ErrorCode::limit_exceeded);
    }
}

void test_loopback_framing_and_state() {
    auto listener = Socket::listen_on({"127.0.0.1", 0});
    const auto port = local_port(listener);
    auto client_socket = Socket::connect_to({"127.0.0.1", port});
    auto server_socket = listener.accept();

    SessionStateMachine client_state(EndpointRole::client);
    SessionStateMachine server_state(EndpointRole::server);
    const Message start = StartRun{1,
                                   1,
                                   AlgorithmId::falcon_512,
                                   limits::payload_10_kib,
                                   limits::iterations_per_run};
    client_state.on_send(start);
    write_message(client_socket, start);
    const auto received_start = read_message(server_socket);
    server_state.on_receive(received_start);
    expect(std::get<StartRun>(received_start).run_id == 1,
           "framed START_RUN changed during loopback transfer");

    const Message ack = StartRunAck{1, StartRunStatus::server_busy};
    server_state.on_send(ack);
    write_message(server_socket, ack);
    const auto received_ack = read_message(client_socket);
    client_state.on_receive(received_ack);
    expect(client_state.state() == SessionState::ready &&
               server_state.state() == SessionState::ready,
           "rejected loopback run did not return both peers to ready");

    const Message finish = Finish{};
    client_state.on_send(finish);
    write_message(client_socket, finish);
    server_state.on_receive(read_message(server_socket));
    expect(client_state.state() == SessionState::finished &&
               server_state.state() == SessionState::finished,
           "loopback FINISH did not terminate both state machines");
}

void test_socket_ownership() {
    auto sockets = make_socket_pair();
    const auto original_handle = sockets.first.native_handle();
    Socket moved = std::move(sockets.first);
    expect(!sockets.first.is_open() && moved.is_open() &&
               moved.native_handle() == original_handle,
           "moving a Socket did not transfer descriptor ownership");

    moved.close();
    expect(!moved.is_open(), "close did not invalidate the socket");
    expect_network_error(
        [&moved] { (void)read_exact(moved, 1); },
        NetworkErrorCode::invalid_argument);
}

}  // namespace

int main() {
    test_large_exact_transfer();
    test_disconnect_and_header_limit();
    test_loopback_framing_and_state();
    test_socket_ownership();
    std::cout << "socket I/O tests passed\n";
}
