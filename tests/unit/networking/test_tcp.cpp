#include <cassert>
#include <iostream>
#include <thread>
#include "dibyt/networking/tcp.hpp"

// Test suite for RAII TCP transport and loopback frame exchange
int main() {
    std::cout << "[TEST] Running TCP Transport & Connection Lifecycle Tests...\n";

    dibyt::networking::init_network_subsystem();

    // 1. Start Server on ephemeral loopback port
    dibyt::networking::TcpServer server;
    auto err = server.start("127.0.0.1", 0, 16);
    assert(err == dibyt::core::ErrorCode::kOk);
    uint16_t port = server.bound_port();
    assert(port > 0);

    // 2. Client Thread connects and exchanges a binary frame
    std::thread client_thread([port]() {
        dibyt::networking::TcpConnection client;
        auto conn_err = dibyt::networking::TcpConnection::connect_to(
            "127.0.0.1", port, std::chrono::milliseconds(2000), client
        );
        assert(conn_err == dibyt::core::ErrorCode::kOk);
        assert(client.is_open());

        // Send a test frame
        dibyt::protocol::Frame send_f;
        send_f.header.msg_type = dibyt::protocol::MessageType::kHeartbeat;
        send_f.header.request_id = 99;
        std::string payload = "ping_payload";
        send_f.payload.assign(payload.begin(), payload.end());
        assert(client.send_frame(send_f) == dibyt::core::ErrorCode::kOk);

        // Receive response frame
        dibyt::protocol::Frame resp_f;
        assert(client.recv_frame(resp_f) == dibyt::core::ErrorCode::kOk);
        assert(resp_f.header.msg_type == dibyt::protocol::MessageType::kHeartbeat);
        assert(resp_f.header.request_id == 100);
        assert(std::string(resp_f.payload.begin(), resp_f.payload.end()) == "pong_payload");

        client.close();
    });

    // 3. Server accepts connection and echoes response
    dibyt::networking::TcpConnection server_conn;
    assert(server.accept_connection(std::chrono::milliseconds(3000), server_conn) == dibyt::core::ErrorCode::kOk);
    assert(server_conn.is_open());

    dibyt::protocol::Frame received_f;
    assert(server_conn.recv_frame(received_f) == dibyt::core::ErrorCode::kOk);
    assert(received_f.header.request_id == 99);
    assert(std::string(received_f.payload.begin(), received_f.payload.end()) == "ping_payload");

    // Reply with pong frame
    dibyt::protocol::Frame reply_f;
    reply_f.header.msg_type = dibyt::protocol::MessageType::kHeartbeat;
    reply_f.header.request_id = 100;
    std::string pong = "pong_payload";
    reply_f.payload.assign(pong.begin(), pong.end());
    assert(server_conn.send_frame(reply_f) == dibyt::core::ErrorCode::kOk);

    client_thread.join();
    server.stop();

    dibyt::networking::shutdown_network_subsystem();

    std::cout << "[SMOKE_TEST_PASSED] TCP transport loopback communication verified successfully.\n";
    return 0;
}
