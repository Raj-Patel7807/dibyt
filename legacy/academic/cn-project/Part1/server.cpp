#include "common.hpp"
#include <csignal>
#include <atomic>
#include <unordered_map>

// Server execution state and synchronization
static std::atomic<bool> g_running{true};
static int g_listener_fd = -1;
static std::atomic<int> g_active_workers{0};

// Topic mapping table per Design Document Section 3.1 & Section 11
static std::unordered_map<std::string, std::string> topic_to_directory;
static std::mutex topic_map_mutex;

// Signal handler for clean and graceful shutdown per Section 9
static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_running = false;
    if (g_listener_fd >= 0) {
        // Shutdown listener to unblock accept()
        ::shutdown(g_listener_fd, SHUT_RDWR);
        ::close(g_listener_fd);
        g_listener_fd = -1;
    }
}

// 10.1 Key Function: Parse command line arguments
static bool parse_arguments(int argc, char **argv, int &port, fs::path &root, bool &multithreaded) {
    if (argc < 3) {
        return false;
    }

    port = std::atoi(argv[1]);
    if (port < 1 || port > 65535) {
        std::cerr << "Error: invalid port number " << argv[1] << "\n";
        return false;
    }

    root = fs::absolute(argv[2]);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Error: invalid topics_root_dir " << argv[2] << "\n";
        return false;
    }

    // Default concurrency mode: single-threaded baseline per Section 3.3,
    // or multithreaded if compiled with MULTITHREADED_DEFAULT
#ifdef MULTITHREADED_DEFAULT
    multithreaded = true;
#else
    multithreaded = false;
#endif

    // Check optional CLI flags
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--multithreaded" || arg == "-m" || arg == "-t") {
            multithreaded = true;
        } else if (arg == "--single-threaded" || arg == "-s") {
            multithreaded = false;
        }
    }

    // Check environment variable for backward compatibility
    const char *env_mode = std::getenv("IT305_MULTITHREADED");
    if (env_mode && std::string(env_mode) == "1") {
        multithreaded = true;
    }

    return true;
}

// 10.1 Key Function: Create and configure listening socket
static int create_server_socket(int port) {
    return listen_on(port);
}

// 10.1 Key Function: Accept an incoming client connection
static int accept_client(int listener_fd, std::string &client_ip, int &client_port) {
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    int fd = ::accept(listener_fd, reinterpret_cast<sockaddr*>(&peer), &len);
    if (fd < 0) {
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &peer.sin_addr, ip_str, sizeof(ip_str));
    client_ip = ip_str;
    client_port = ntohs(peer.sin_port);
    return fd;
}

// Initialize topic map from topics_root_dir per Section 3.1
static void init_topic_map(const fs::path &root) {
    std::lock_guard<std::mutex> lk(topic_map_mutex);
    topic_to_directory.clear();
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(root, ec)) {
        if (!ec && entry.is_directory()) {
            std::string topic_name = entry.path().filename().string();
            if (safe_name(topic_name)) {
                topic_to_directory[topic_name] = entry.path().string();
            }
        }
    }
}

// 10.1 Key Function: Resolve requested topic to directory path using hash map
static bool resolve_topic(const std::string &topic, std::string &dir_path) {
    if (!safe_name(topic)) {
        return false;
    }
    std::lock_guard<std::mutex> lk(topic_map_mutex);
    auto it = topic_to_directory.find(topic);
    if (it != topic_to_directory.end()) {
        dir_path = it->second;
        return true;
    }
    return false;
}

// 10.1 Key Function: List regular files in topic directory
static std::vector<fs::path> list_files(const fs::path &dir) {
    return regular_files(dir);
}

// Send standard ERROR message per Section 3.2
static void send_error(int fd, const std::string &code, const std::string &msg) {
    logline("ERROR fd=" + std::to_string(fd) + " code=" + code + " msg=" + msg);
    send_line(fd, "ERROR " + code + " " + msg + "\n");
}

// 10.1 Key Function: Send a single file according to Section 3.2 binary stream format
static bool send_file(int fd, const fs::path &path) {
    std::string name = path.filename().string();
    std::error_code ec;
    uint64_t size = fs::file_size(path, ec);
    if (ec) {
        logline("FILE_STAT_ERROR fd=" + std::to_string(fd) + " file=" + name + " error=" + ec.message());
        return false;
    }

    logline("FILE_START fd=" + std::to_string(fd) + " file=" + name + " size=" + std::to_string(size));

    // Section 3.2 Header: FILE <name_length> <file_size>\n
    std::string header = "FILE " + std::to_string(name.size()) + " " + std::to_string(size) + "\n";
    if (!send_all(fd, header.data(), header.size())) {
        return false;
    }

    // Section 3.2 Filename bytes: <name bytes>
    if (!send_all(fd, name.data(), name.size())) {
        return false;
    }

    // Section 3.2 File content bytes: <exactly file_size data bytes>
    if (size > 0) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            logline("FILE_OPEN_ERROR fd=" + std::to_string(fd) + " file=" + name);
            return false;
        }

        char buf[CHUNK_SIZE];
        uint64_t remaining = size;
        while (remaining > 0) {
            size_t want = static_cast<size_t>(std::min<uint64_t>(CHUNK_SIZE, remaining));
            in.read(buf, static_cast<std::streamsize>(want));
            size_t got = static_cast<size_t>(in.gcount());
            if (got == 0) {
                logline("FILE_READ_UNEXPECTED_EOF fd=" + std::to_string(fd) + " file=" + name);
                return false;
            }
            if (!send_all(fd, buf, got)) {
                return false;
            }
            remaining -= got;
        }
    }

    logline("FILE_COMPLETE fd=" + std::to_string(fd) + " file=" + name + " bytes_sent=" + std::to_string(size));
    return true;
}

// 10.1 Key Function: Handle client connection and topic distribution
static void handle_client(int fd, const std::string &client_ip, int client_port) {
    g_active_workers++;
    logline("CLIENT_CONNECT fd=" + std::to_string(fd) + " peer=" + client_ip + ":" + std::to_string(client_port));

    std::string line;
    if (!recv_line(fd, line)) {
        logline("CLIENT_DISCONNECT_PREMATURE fd=" + std::to_string(fd));
        ::close(fd);
        g_active_workers--;
        return;
    }

    auto tokens = split(line);
    if (tokens.size() < 2 || tokens[0] != "GET") {
        send_error(fd, "BAD_REQUEST", "expected_GET_topic");
        ::close(fd);
        g_active_workers--;
        return;
    }

    std::string topic = tokens[1];
    logline("REQUEST_RECEIVED fd=" + std::to_string(fd) + " topic=" + topic);

    std::string topic_dir;
    if (!resolve_topic(topic, topic_dir)) {
        send_error(fd, "UNKNOWN_TOPIC", topic);
        ::close(fd);
        g_active_workers--;
        return;
    }

    auto files = list_files(topic_dir);

    // Section 3.2 Response: OK <topic> <file_count>\n
    std::string ok_msg = "OK " + topic + " " + std::to_string(files.size()) + "\n";
    if (!send_line(fd, ok_msg)) {
        logline("SEND_OK_FAILED fd=" + std::to_string(fd));
        ::close(fd);
        g_active_workers--;
        return;
    }

    // Immediately stream files per Section 3.2 (no intermediate START handshake)
    bool success = true;
    for (const auto &path : files) {
        if (!send_file(fd, path)) {
            logline("TRANSFER_FAILED_ABORT fd=" + std::to_string(fd) + " file=" + path.filename().string());
            success = false;
            break;
        }
    }

    if (success) {
        // Section 3.2 Termination: END\n
        if (send_line(fd, "END\n")) {
            logline("TRANSFER_COMPLETE fd=" + std::to_string(fd) + " topic=" + topic +
                    " files=" + std::to_string(files.size()));
        } else {
            logline("SEND_END_FAILED fd=" + std::to_string(fd));
        }
    }

    ::close(fd);
    g_active_workers--;
}

int main(int argc, char **argv) {
    int port = 0;
    fs::path root;
    bool multithreaded = false;

    if (!parse_arguments(argc, argv, port, root, multithreaded)) {
        std::cerr << "Usage: " << argv[0] << " <port> <topics_root_dir> [--multithreaded|--single-threaded]\n";
        return 2;
    }

    // Install signal handlers for clean shutdown per Section 9
    std::signal(SIGINT, handle_shutdown_signal);
    std::signal(SIGTERM, handle_shutdown_signal);

    // Initialize topic map per Section 3.1
    init_topic_map(root);

    int listener = create_server_socket(port);
    if (listener < 0) {
        std::perror("listen_on");
        return 1;
    }
    g_listener_fd = listener;

    logline("SERVER_START mode=" + std::string(multithreaded ? "multithreaded" : "single-threaded") +
            " port=" + std::to_string(port) + " root=" + root.string() +
            " topics_loaded=" + std::to_string(topic_to_directory.size()));

    while (g_running) {
        std::string client_ip;
        int client_port = 0;
        int client_fd = accept_client(g_listener_fd, client_ip, client_port);
        if (client_fd < 0) {
            if (!g_running) break;
            if (errno == EINTR) continue;
            std::perror("accept_client");
            continue;
        }

        if (multithreaded) {
            // Multi-threaded server: create worker thread per client per Section 3.4
            std::thread([client_fd, client_ip, client_port]() {
                handle_client(client_fd, client_ip, client_port);
            }).detach();
        } else {
            // Single-threaded server baseline: serve sequentially per Section 3.3
            handle_client(client_fd, client_ip, client_port);
        }
    }

    logline("SERVER_SHUTTING_DOWN active_workers=" + std::to_string(g_active_workers.load()));
    // Wait briefly for in-flight requests to complete
    int wait_cycles = 0;
    while (g_active_workers.load() > 0 && wait_cycles < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_cycles++;
    }

    logline("SERVER_STOPPED");
    return 0;
}
