#include "common.hpp"

// 10.1 Key Function: Parse command-line arguments for client
static bool parse_arguments(int argc, char **argv, std::string &server_ip, int &server_port,
                            std::string &topic, fs::path &output_dir) {
    if (argc != 5) {
        return false;
    }

    server_ip = argv[1];
    server_port = std::atoi(argv[2]);
    if (server_port < 1 || server_port > 65535) {
        std::cerr << "Error: invalid port number " << argv[2] << "\n";
        return false;
    }

    topic = argv[3];
    if (!safe_name(topic)) {
        std::cerr << "Error: invalid topic name " << argv[3] << "\n";
        return false;
    }

    output_dir = fs::path(argv[4]);
    return true;
}

// 10.1 Key Function: Record and output performance metrics per Section 3.5 & Section 8
static void record_metrics(const std::string &topic, uint64_t bytes, double elapsed_seconds) {
    double throughput = bytes / elapsed_seconds;
    std::cout << "TRANSFER_OK topic=" << topic
              << " useful_bytes=" << bytes
              << " elapsed_seconds=" << elapsed_seconds
              << " throughput_Bps=" << throughput << "\n";
}

// 10.1 Key Function: Receive a single file according to Section 3.2 binary stream format
static bool receive_file(int fd, const std::string &header_line, const fs::path &target_dir,
                         uint64_t &bytes_received) {
    auto tokens = split(header_line);
    // Section 3.2 Header: FILE <name_length> <file_size>
    if (tokens.size() != 3 || tokens[0] != "FILE") {
        std::cerr << "Protocol error: expected 'FILE <name_length> <file_size>', got: " << header_line << "\n";
        return false;
    }

    size_t name_length = 0;
    uint64_t file_size = 0;
    try {
        name_length = std::stoull(tokens[1]);
        file_size = std::stoull(tokens[2]);
    } catch (...) {
        std::cerr << "Protocol error: invalid numbers in FILE line: " << header_line << "\n";
        return false;
    }

    if (name_length == 0 || name_length > 4096) {
        std::cerr << "Protocol error: invalid filename length " << name_length << "\n";
        return false;
    }

    // Section 3.2 Filename bytes: <name bytes>
    std::string filename(name_length, '\0');
    if (!recv_exact(fd, filename.data(), name_length) || !safe_name(filename)) {
        std::cerr << "Protocol error: failed to read valid filename of length " << name_length << "\n";
        return false;
    }

    fs::path dest = target_dir / filename;
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Error: failed to open output file for writing: " << dest.string() << "\n";
        return false;
    }

    // Section 3.2 File content bytes: <exactly file_size data bytes>
    uint64_t remaining = file_size;
    char buf[CHUNK_SIZE];
    while (remaining > 0) {
        size_t want = static_cast<size_t>(std::min<uint64_t>(CHUNK_SIZE, remaining));
        if (!recv_exact(fd, buf, want)) {
            std::cerr << "Connection error: unexpected EOF while reading file content for " << filename << "\n";
            out.close();
            std::error_code ec;
            fs::remove(dest, ec); // Remove partially downloaded file
            return false;
        }

        out.write(buf, static_cast<std::streamsize>(want));
        if (!out) {
            std::cerr << "Error: failed writing bytes to file: " << dest.string() << "\n";
            out.close();
            std::error_code ec;
            fs::remove(dest, ec);
            return false;
        }
        remaining -= want;
    }

    out.close();
    bytes_received += file_size;
    return true;
}

// Execute file transfer request
static bool run_client(const std::string &server_ip, int server_port,
                       const std::string &topic, const fs::path &output_dir) {
    fs::path target_dir = output_dir / topic;
    std::error_code ec;
    fs::create_directories(target_dir, ec);
    if (ec) {
        std::cerr << "Error: failed to create target directory " << target_dir.string() << ": " << ec.message() << "\n";
        return false;
    }

    double start_time = now_seconds();

    int fd = connect_to(server_ip, server_port);
    if (fd < 0) {
        std::cerr << "Connection failed: unable to connect to " << server_ip << ":" << server_port << "\n";
        return false;
    }

    // Section 3.2 Request: GET <topic>\n
    std::string req = "GET " + topic + "\n";
    if (!send_line(fd, req)) {
        std::cerr << "Send error: failed to send GET request to server\n";
        ::close(fd);
        return false;
    }

    // Section 3.2 Response: OK <topic> <file_count>\n or ERROR <code> <message>\n
    std::string resp_line;
    if (!recv_line(fd, resp_line)) {
        std::cerr << "Connection error: server closed connection before sending response\n";
        ::close(fd);
        return false;
    }

    auto tokens = split(resp_line);
    if (tokens.empty()) {
        std::cerr << "Protocol error: received empty response line from server\n";
        ::close(fd);
        return false;
    }

    if (tokens[0] == "ERROR") {
        std::cerr << "Server error: " << resp_line << "\n";
        ::close(fd);
        return false;
    }

    if (tokens.size() < 3 || tokens[0] != "OK" || tokens[1] != topic) {
        std::cerr << "Protocol error: unexpected response: " << resp_line << "\n";
        ::close(fd);
        return false;
    }

    size_t expected_files = 0;
    try {
        expected_files = std::stoull(tokens[2]);
    } catch (...) {
        std::cerr << "Protocol error: invalid file count in response: " << resp_line << "\n";
        ::close(fd);
        return false;
    }

    uint64_t total_bytes = 0;
    size_t files_received = 0;
    bool completed_normally = false;

    // Receive file entries directly without intermediate START handshake
    while (recv_line(fd, resp_line)) {
        if (resp_line == "END") {
            completed_normally = true;
            break;
        }

        if (resp_line.rfind("ERROR", 0) == 0) {
            std::cerr << "Server error during transfer: " << resp_line << "\n";
            ::close(fd);
            return false;
        }

        if (!receive_file(fd, resp_line, target_dir, total_bytes)) {
            ::close(fd);
            return false;
        }
        files_received++;
    }

    ::close(fd);

    if (!completed_normally) {
        std::cerr << "Transfer error: connection closed before receiving END\n";
        return false;
    }

    if (files_received != expected_files) {
        std::cerr << "Warning: received " << files_received << " files, expected " << expected_files << "\n";
    }

    double elapsed_seconds = std::max(0.000001, now_seconds() - start_time);
    record_metrics(topic, total_bytes, elapsed_seconds);
    return true;
}

int main(int argc, char **argv) {
    std::string server_ip;
    int server_port = 0;
    std::string topic;
    fs::path output_dir;

    if (!parse_arguments(argc, argv, server_ip, server_port, topic, output_dir)) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <topic> <output_dir>\n";
        return 2;
    }

    bool success = run_client(server_ip, server_port, topic, output_dir);
    return success ? 0 : 1;
}
