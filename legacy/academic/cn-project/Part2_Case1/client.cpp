#include "common.hpp"

static bool run_client(const std::string &ip, int port, const std::string &topic, const fs::path &out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::path target = out / topic;
    fs::create_directories(target, ec);

    int max_retries = 50;
    if (const char *env = std::getenv("IT305_MAX_RETRIES")) {
        try {
            int v = std::stoi(env);
            if (v > 0) max_retries = v;
        } catch (...) {}
    }

    int base_delay_ms = 50;
    if (const char *env = std::getenv("IT305_RETRY_DELAY_MS")) {
        try {
            int v = std::stoi(env);
            if (v >= 0) base_delay_ms = v;
        } catch (...) {}
    }

    double overall_start = now_seconds();
    uint64_t total_bytes_streamed = 0;

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        std::cerr << "[CLIENT] Attempt " << attempt << "/" << max_retries
                  << ": Connecting to " << ip << ":" << port
                  << " for topic '" << topic << "'...\n";

        int fd = connect_to(ip, port);
        if (fd < 0) {
            int delay = std::min(1000, base_delay_ms * attempt);
            std::cerr << "[CLIENT] Connection failed (server unreachable). Retrying in "
                      << delay << "ms...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            continue;
        }

        // Send GET request for the topic (Case 1: never uses session or resume headers)
        std::string req = "GET " + topic + "\n";
        if (!send_line(fd, req)) {
            std::cerr << "[CLIENT] Failed to send GET request to server.\n";
            close(fd);
            continue;
        }

        std::string line;
        if (!recv_line(fd, line)) {
            std::cerr << "[CLIENT] Connection closed before receiving response header.\n";
            close(fd);
            continue;
        }

        auto w = split(line);
        if (!w.empty() && w[0] == "ERROR") {
            std::cerr << "[CLIENT] Fatal server error: " << line << "\n";
            close(fd);
            return false;
        }

        if (w.size() < 3 || w[0] != "OK" || w[1] != topic) {
            std::cerr << "[CLIENT] Protocol error: unexpected response '" << line << "'\n";
            close(fd);
            return false;
        }

        size_t expected_files = 0;
        try {
            expected_files = std::stoull(w[2]);
        } catch (...) {
            std::cerr << "[CLIENT] Invalid file count from server: " << w[2] << "\n";
            close(fd);
            return false;
        }

        std::cerr << "[CLIENT] Server confirmed topic '" << topic << "' ("
                  << expected_files << " files). Starting transfer...\n";

        if (!send_line(fd, "START\n")) {
            std::cerr << "[CLIENT] Failed to send START control message.\n";
            close(fd);
            continue;
        }

        bool failed = false;
        bool transfer_done = false;
        std::string current_file;
        uint64_t current_file_bytes = 0;
        size_t files_received = 0;

        while (recv_line(fd, line)) {
            w = split(line);
            if (w.empty()) {
                failed = true;
                break;
            }
            if (w[0] == "END") {
                transfer_done = true;
                break;
            }
            if (w[0] == "ERROR") {
                std::cerr << "[CLIENT] Server reported error mid-transfer: " << line << "\n";
                close(fd);
                return false;
            }
            if (w[0] != "FILE" || w.size() != 4) {
                std::cerr << "[CLIENT] Unexpected line framing: " << line << "\n";
                failed = true;
                break;
            }

            size_t nlen = 0;
            uint64_t file_size = 0, offset = 0;
            try {
                nlen = std::stoull(w[1]);
                file_size = std::stoull(w[2]);
                offset = std::stoull(w[3]);
            } catch (...) {
                failed = true;
                break;
            }

            if (nlen == 0 || nlen > 4096 || offset != 0) {
                std::cerr << "[CLIENT] Invalid file metadata header\n";
                failed = true;
                break;
            }

            std::string name(nlen, '\0');
            if (!recv_exact(fd, name.data(), nlen) || !safe_name(name)) {
                std::cerr << "[CLIENT] Failed to read filename or filename unsafe\n";
                failed = true;
                break;
            }

            current_file = name;
            current_file_bytes = 0;
            fs::path dest = target / name;

            std::ofstream file(dest, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                std::cerr << "[CLIENT] Failed to create local file: " << dest.string() << "\n";
                failed = true;
                break;
            }

            uint64_t pos = 0;
            while (pos < file_size) {
                if (!recv_line(fd, line)) {
                    failed = true;
                    break;
                }
                w = split(line);
                if (w.size() != 3 || w[0] != "DATA") {
                    failed = true;
                    break;
                }

                uint64_t chunk_off = 0;
                size_t chunk_bytes = 0;
                try {
                    chunk_off = std::stoull(w[1]);
                    chunk_bytes = std::stoull(w[2]);
                } catch (...) {
                    failed = true;
                    break;
                }

                if (chunk_off != pos || chunk_bytes == 0 ||
                    chunk_bytes > CHUNK_SIZE || pos + chunk_bytes > file_size) {
                    failed = true;
                    break;
                }

                std::vector<char> buf(chunk_bytes);
                if (!recv_exact(fd, buf.data(), chunk_bytes)) {
                    failed = true;
                    break;
                }

                file.write(buf.data(), static_cast<std::streamsize>(chunk_bytes));
                if (!file.good()) {
                    std::cerr << "[CLIENT] Local disk write error for file: " << name << "\n";
                    failed = true;
                    break;
                }

                pos += chunk_bytes;
                current_file_bytes = pos;
                total_bytes_streamed += chunk_bytes;
            }

            file.close();
            if (failed) break;
            files_received++;
        }

        close(fd);

        if (transfer_done && files_received == expected_files) {
            double elapsed = std::max(0.000001, now_seconds() - overall_start);
            uint64_t useful_bytes = 0;
            for (const auto &p : fs::directory_iterator(target, ec)) {
                if (p.is_regular_file()) {
                    useful_bytes += p.file_size();
                }
            }

            std::cerr << "[CLIENT] Success: " << files_received << " files ("
                      << useful_bytes << " bytes) transferred in " << attempt
                      << " attempt(s) over " << elapsed << "s ("
                      << (static_cast<double>(useful_bytes) / (1024.0 * 1024.0 * elapsed))
                      << " MB/s)\n";

            std::cout << "TRANSFER_OK topic=" << topic << " part=0 useful_bytes=" << useful_bytes
                      << " elapsed_seconds=" << elapsed
                      << " throughput_Bps=" << (useful_bytes / elapsed)
                      << " attempts=" << attempt << "\n";
            return true;
        }

        // Connection failed mid-transfer
        std::cerr << "[CLIENT] Warning: Connection disconnected prematurely during attempt "
                  << attempt << " (file '" << current_file << "' at offset "
                  << current_file_bytes << ").\n";
        std::cerr << "[CLIENT] Case 1 recovery: Discarding partial downloads in '"
                  << target.string() << "' and restarting from scratch...\n";

        // In Part II Case 1, discard all downloaded files and recreate empty directory
        fs::remove_all(target, ec);
        fs::create_directories(target, ec);

        int delay = std::min(1000, base_delay_ms * attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    std::cerr << "[CLIENT] Error: Failed to complete transfer after " << max_retries << " attempts.\n";
    return false;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <topic> <output_dir>\n";
        return 2;
    }

    std::string ip = argv[1];
    int port = 0;
    try {
        size_t idx = 0;
        port = std::stoi(argv[2], &idx);
        if (idx != std::string(argv[2]).size() || port < 1 || port > 65535) {
            throw std::out_of_range("port");
        }
    } catch (...) {
        std::cerr << "Error: invalid server_port '" << argv[2] << "'. Must be an integer in range [1, 65535].\n";
        return 2;
    }

    std::string topic = argv[3];
    if (!safe_name(topic)) {
        std::cerr << "Error: invalid topic name '" << topic << "'. Must not contain path separators or traversal characters.\n";
        return 2;
    }

    fs::path out = fs::absolute(argv[4]);
    return run_client(ip, port, topic, out) ? 0 : 1;
}
