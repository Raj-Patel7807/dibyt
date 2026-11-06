#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>
#include <span>
#include <memory>
#include "dibyt/core/error.hpp"
#include "dibyt/protocol/codec.hpp"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_handle_t = SOCKET;
    inline constexpr socket_handle_t kInvalidSocketHandle = INVALID_SOCKET;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using socket_handle_t = int;
    inline constexpr socket_handle_t kInvalidSocketHandle = -1;
#endif

namespace dibyt::networking {

// Subsystem initialization (Winsock on Windows, no-op on POSIX)
bool init_network_subsystem() noexcept;
void shutdown_network_subsystem() noexcept;

// Low-level RAII socket handle wrapper
class SocketHandle {
public:
    SocketHandle() noexcept : handle_(kInvalidSocketHandle) {}
    explicit SocketHandle(socket_handle_t handle) noexcept : handle_(handle) {}
    ~SocketHandle() noexcept { close(); }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = kInvalidSocketHandle;
    }

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = kInvalidSocketHandle;
        }
        return *this;
    }

    [[nodiscard]] bool is_valid() const noexcept { return handle_ != kInvalidSocketHandle; }
    [[nodiscard]] socket_handle_t get() const noexcept { return handle_; }

    void close() noexcept;
    socket_handle_t release() noexcept {
        socket_handle_t h = handle_;
        handle_ = kInvalidSocketHandle;
        return h;
    }

private:
    socket_handle_t handle_;
};

struct SocketConfig {
    bool tcp_nodelay{true};
    bool keepalive{true};
    std::chrono::milliseconds timeout{5000}; // 5-second deadline default
};

// Connected client socket endpoint wrapper
class TcpConnection {
public:
    TcpConnection() = default;
    explicit TcpConnection(SocketHandle socket, const SocketConfig& config = {});

    [[nodiscard]] bool is_open() const noexcept { return socket_.is_valid(); }
    void close() noexcept { socket_.close(); }

    // Stream helpers: handle partial operations, EINTR, and timeouts
    [[nodiscard]] core::ErrorCode send_all(std::span<const uint8_t> data) noexcept;
    [[nodiscard]] core::ErrorCode recv_exact(std::span<uint8_t> buffer) noexcept;

    // High-level frame transfer over wire
    [[nodiscard]] core::ErrorCode send_frame(const protocol::Frame& frame);
    [[nodiscard]] core::ErrorCode recv_frame(protocol::Frame& out_frame);

    // Apply socket timeout and keepalive options
    [[nodiscard]] core::ErrorCode configure_options(const SocketConfig& config) noexcept;

    // Connect to remote host
    [[nodiscard]] static core::ErrorCode connect_to(
        std::string_view host, 
        uint16_t port, 
        std::chrono::milliseconds timeout, 
        TcpConnection& out_conn
    );

private:
    SocketHandle socket_;
    SocketConfig config_;
};

// Bounded TCP Server listener with admission control
class TcpServer {
public:
    TcpServer();
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    [[nodiscard]] core::ErrorCode start(
        std::string_view bind_ip, 
        uint16_t port, 
        uint32_t max_concurrency = 64
    );

    // Accept next connection with timeout (returns kOk if client connected, kNetworkError on timeout)
    [[nodiscard]] core::ErrorCode accept_connection(
        std::chrono::milliseconds timeout, 
        TcpConnection& out_conn
    );

    void stop() noexcept;
    [[nodiscard]] bool is_running() const noexcept { return is_running_; }
    [[nodiscard]] uint16_t bound_port() const noexcept { return bound_port_; }

private:
    SocketHandle listen_socket_;
    bool is_running_{false};
    uint16_t bound_port_{0};
    uint32_t max_concurrency_{64};
};

} // namespace dibyt::networking
