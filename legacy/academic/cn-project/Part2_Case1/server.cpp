#include "common.hpp"

static std::mt19937_64 rng(std::random_device{}());
static std::mutex rng_mutex;

static bool should_fail(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    std::lock_guard<std::mutex> lk(rng_mutex);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < p;
}

static void send_error(int fd, const std::string &code, const std::string &msg) {
    send_line(fd, "ERROR " + code + " " + msg + "\n");
}

static void serve_client(int fd, fs::path root, double failure_p) {
    std::string line;
    if (!recv_line(fd, line)) {
        close(fd);
        return;
    }
    auto w = split(line);
    if (w.size() < 2 || w[0] != "GET" || !safe_name(w[1])) {
        send_error(fd, "BAD_REQUEST", "expected_GET_topic");
        close(fd);
        return;
    }
    std::string topic = w[1];
    fs::path dir = root / topic;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        send_error(fd, "UNKNOWN_TOPIC", topic);
        close(fd);
        return;
    }

    auto files = regular_files(dir);
    if (!send_line(fd, "OK " + topic + " " + std::to_string(files.size()) + "\n")) {
        close(fd);
        return;
    }

    if (!recv_line(fd, line)) {
        close(fd);
        return;
    }
    w = split(line);
    if (w.empty() || w[0] != "START") {
        send_error(fd, "BAD_START", "expected_START");
        close(fd);
        return;
    }

    for (const auto &path : files) {
        std::string name = path.filename().string();
        std::error_code ec;
        uint64_t size = fs::file_size(path, ec);
        if (ec) {
            send_error(fd, "STAT_FAILED", name);
            close(fd);
            return;
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            send_error(fd, "OPEN_FAILED", name);
            close(fd);
            return;
        }

        // Part II Case 1 has no session/resume state; always stream from byte 0
        uint64_t offset = 0;
        if (!send_line(fd, "FILE " + std::to_string(name.size()) + " " +
                           std::to_string(size) + " " + std::to_string(offset) + "\n") ||
            !send_all(fd, name.data(), name.size())) {
            in.close();
            close(fd);
            return;
        }

        uint64_t pos = 0;
        char buf[CHUNK_SIZE];
        while (pos < size) {
            size_t want = static_cast<size_t>(std::min<uint64_t>(CHUNK_SIZE, size - pos));
            in.read(buf, static_cast<std::streamsize>(want));
            size_t got = static_cast<size_t>(in.gcount());
            if (got == 0) {
                in.close();
                close(fd);
                return;
            }

            if (!send_line(fd, "DATA " + std::to_string(pos) + " " + std::to_string(got) + "\n") ||
                !send_all(fd, buf, got)) {
                in.close();
                close(fd);
                return;
            }
            pos += got;

            // Fault injection decision after each transmitted data chunk
            if (should_fail(failure_p)) {
                logline("FAULT INJECTED: topic=" + topic + " file=" + name +
                        " offset=" + std::to_string(pos) + "/" + std::to_string(size) +
                        " (p=" + std::to_string(failure_p) + ")");
                in.close();
                close(fd);
                return;
            }
        }
        in.close();
    }

    send_line(fd, "END\n");
    close(fd);
    logline("Transfer complete: topic=" + topic + " (" + std::to_string(files.size()) + " files)");
}

int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <port> <topics_root_dir> <failure_probability>\n";
        return 2;
    }

    int port = 0;
    try {
        size_t idx = 0;
        port = std::stoi(argv[1], &idx);
        if (idx != std::string(argv[1]).size() || port < 1 || port > 65535) {
            throw std::out_of_range("port");
        }
    } catch (...) {
        std::cerr << "Error: invalid port '" << argv[1] << "'. Must be an integer in range [1, 65535].\n";
        return 2;
    }

    fs::path root = fs::absolute(argv[2]);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Error: topics_root_dir '" << argv[2] << "' does not exist or is not a directory.\n";
        return 2;
    }

    double p = 0.0;
    try {
        size_t idx = 0;
        p = std::stod(argv[3], &idx);
        if (idx != std::string(argv[3]).size() || p < 0.0 || p > 1.0) {
            throw std::out_of_range("probability");
        }
    } catch (...) {
        std::cerr << "Error: failure_probability must be a number in range [0.0, 1.0].\n";
        return 2;
    }

    int listener = listen_on(port);
    if (listener < 0) {
        perror("listen_on");
        return 1;
    }

    logline("Part2_Case1 server listening on port " + std::to_string(port) +
            ", root=" + root.string() + ", failure_probability=" + std::to_string(p));

    while (true) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        int fd = accept(listener, reinterpret_cast<sockaddr*>(&peer), &len);
        if (fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        // Multi-threaded worker model: one detached thread per client
        std::thread(serve_client, fd, root, p).detach();
    }
}
