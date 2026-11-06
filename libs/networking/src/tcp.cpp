#include "dibyt/networking/tcp.hpp"
#include <cstring>
#include <iostream>

#if defined(_MSC_VER)
    #pragma comment(lib, "ws2_32.lib")
#elif !defined(_WIN32)
    #include <poll.h>
    #include <errno.h>
#endif

namespace dibyt::networking {

bool init_network_subsystem() noexcept {
#if defined(_WIN32)
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
#else
    return true;
#endif
}

void shutdown_network_subsystem() noexcept {
#if defined(_WIN32)
    WSACleanup();
#endif
}

void SocketHandle::close() noexcept {
    if (handle_ != kInvalidSocketHandle) {
#if defined(_WIN32)
        closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = kInvalidSocketHandle;
    }
}

TcpConnection::TcpConnection(SocketHandle socket, const SocketConfig& config)
    : socket_(std::move(socket)), config_(config) {
    if (socket_.is_valid()) {
        (void)configure_options(config_);
    }
}

core::ErrorCode TcpConnection::configure_options(const SocketConfig& config) noexcept {
    if (!socket_.is_valid()) {
        return core::ErrorCode::kInvalidArgument;
    }
    config_ = config;
    socket_handle_t fd = socket_.get();

    // TCP_NODELAY
    int nodelay = config.tcp_nodelay ? 1 : 0;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    // SO_KEEPALIVE
    int keepalive = config.keepalive ? 1 : 0;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepalive), sizeof(keepalive));

    // Timeouts
#if defined(_WIN32)
    DWORD timeout_ms = static_cast<DWORD>(config.timeout.count());
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    struct timeval tv;
    tv.tv_sec = config.timeout.count() / 1000;
    tv.tv_usec = (config.timeout.count() % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif

    return core::ErrorCode::kOk;
}

core::ErrorCode TcpConnection::send_all(std::span<const uint8_t> data) noexcept {
    if (!socket_.is_valid()) {
        return core::ErrorCode::kNetworkError;
    }
    size_t total_sent = 0;
    const size_t length = data.size();
    socket_handle_t fd = socket_.get();

    while (total_sent < length) {
#if defined(_WIN32)
        int sent = send(fd, reinterpret_cast<const char*>(data.data() + total_sent), 
                        static_cast<int>(length - total_sent), 0);
        if (sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            return core::ErrorCode::kNetworkError;
        }
#else
        ssize_t sent = ::send(fd, data.data() + total_sent, length - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return core::ErrorCode::kNetworkError;
        }
#endif
        if (sent == 0) {
            return core::ErrorCode::kNetworkError;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode TcpConnection::recv_exact(std::span<uint8_t> buffer) noexcept {
    if (!socket_.is_valid()) {
        return core::ErrorCode::kNetworkError;
    }
    size_t total_read = 0;
    const size_t length = buffer.size();
    socket_handle_t fd = socket_.get();

    while (total_read < length) {
#if defined(_WIN32)
        int r = recv(fd, reinterpret_cast<char*>(buffer.data() + total_read), 
                     static_cast<int>(length - total_read), 0);
        if (r == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEINTR) continue;
            return core::ErrorCode::kNetworkError;
        }
#else
        ssize_t r = ::recv(fd, buffer.data() + total_read, length - total_read, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return core::ErrorCode::kNetworkError;
        }
#endif
        if (r == 0) {
            // Unexpected EOF mid-stream
            return core::ErrorCode::kNetworkError;
        }
        total_read += static_cast<size_t>(r);
    }
    return core::ErrorCode::kOk;
}

core::ErrorCode TcpConnection::send_frame(const protocol::Frame& frame) {
    std::vector<uint8_t> wire;
    protocol::encode_frame(frame, wire);
    return send_all(wire);
}

core::ErrorCode TcpConnection::recv_frame(protocol::Frame& out_frame) {
    // 1. Read fixed 24-byte header
    uint8_t header_bytes[protocol::kHeaderSizeBytes];
    auto err = recv_exact(header_bytes);
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    // 2. Decode and validate header
    std::span<const uint8_t, protocol::kHeaderSizeBytes> hdr_span(header_bytes, protocol::kHeaderSizeBytes);
    err = protocol::decode_header(hdr_span, out_frame.header);
    if (err != core::ErrorCode::kOk) {
        return err;
    }

    // 3. Read payload according to bounded payload length
    uint32_t payload_len = out_frame.header.payload_length;
    out_frame.payload.resize(payload_len);
    if (payload_len > 0) {
        err = recv_exact(out_frame.payload);
        if (err != core::ErrorCode::kOk) {
            return err;
        }

        // Verify CRC32
        uint32_t calculated_crc = protocol::compute_crc32(out_frame.payload);
        if (calculated_crc != out_frame.header.crc32) {
            return core::ErrorCode::kIntegrityMismatch;
        }
    }

    return core::ErrorCode::kOk;
}

core::ErrorCode TcpConnection::connect_to(
    std::string_view host, 
    uint16_t port, 
    std::chrono::milliseconds timeout, 
    TcpConnection& out_conn
) {
    init_network_subsystem();

    socket_handle_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kInvalidSocketHandle) {
        return core::ErrorCode::kNetworkError;
    }

    SocketHandle sock(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string host_str(host);
    if (inet_pton(AF_INET, host_str.c_str(), &addr.sin_addr) <= 0) {
        return core::ErrorCode::kInvalidArgument;
    }

    if (connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        return core::ErrorCode::kNetworkError;
    }

    SocketConfig config;
    config.timeout = timeout;
    out_conn = TcpConnection(std::move(sock), config);
    return core::ErrorCode::kOk;
}

TcpServer::TcpServer() {
    init_network_subsystem();
}

TcpServer::~TcpServer() {
    stop();
}

core::ErrorCode TcpServer::start(std::string_view bind_ip, uint16_t port, uint32_t max_concurrency) {
    stop();
    max_concurrency_ = max_concurrency;

    socket_handle_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == kInvalidSocketHandle) {
        return core::ErrorCode::kNetworkError;
    }

    listen_socket_ = SocketHandle(fd);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string ip_str(bind_ip);
    if (inet_pton(AF_INET, ip_str.c_str(), &addr.sin_addr) <= 0) {
        return core::ErrorCode::kInvalidArgument;
    }

    if (bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        return core::ErrorCode::kNetworkError;
    }

    // Listen with bounded backlog
    int backlog = static_cast<int>(max_concurrency_);
    if (listen(fd, backlog) != 0) {
        return core::ErrorCode::kNetworkError;
    }

    // Resolve bound port (especially if port 0 was passed)
    socklen_t len = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        bound_port_ = ntohs(addr.sin_port);
    } else {
        bound_port_ = port;
    }

    is_running_ = true;
    return core::ErrorCode::kOk;
}

core::ErrorCode TcpServer::accept_connection(std::chrono::milliseconds timeout, TcpConnection& out_conn) {
    if (!is_running_ || !listen_socket_.is_valid()) {
        return core::ErrorCode::kNetworkError;
    }

    socket_handle_t lfd = listen_socket_.get();

    // Select/poll for timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(lfd, &read_fds);

    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    int sel = select(static_cast<int>(lfd + 1), &read_fds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        return core::ErrorCode::kNetworkError; // Timeout or error
    }

    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    socket_handle_t client_fd = accept(lfd, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd == kInvalidSocketHandle) {
        return core::ErrorCode::kNetworkError;
    }

    SocketHandle client_sock(client_fd);
    out_conn = TcpConnection(std::move(client_sock));
    return core::ErrorCode::kOk;
}

void TcpServer::stop() noexcept {
    is_running_ = false;
    listen_socket_.close();
}

} // namespace dibyt::networking
