#include "common.hpp"

enum class SessionStatus {
    ACTIVE,
    COMPLETED
};

struct ServerSession {
    std::string session_id;
    std::string topic;
    std::string current_file;
    uint64_t file_size = 0;
    uint64_t validated_checkpoint_offset = 0;
    SessionStatus status = SessionStatus::ACTIVE;
    std::set<std::string> completed_files;
    std::chrono::steady_clock::time_point last_active;
};

static std::map<std::string, ServerSession> server_sessions;
static std::mutex sessions_mutex;

static std::mt19937_64 rng(std::random_device{}());
static std::mutex rng_mutex;

static bool should_fail(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    std::lock_guard<std::mutex> lk(rng_mutex);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < p;
}

static std::string new_session_id() {
    std::lock_guard<std::mutex> lk(rng_mutex);
    std::ostringstream o;
    o << "S" << std::hex << rng() << rng();
    return o.str();
}

static void send_error(int fd, const std::string &code, const std::string &msg) {
    send_line(fd, "ERROR " + code + " " + msg + "\n");
}

static size_t get_checkpoint_interval(double failure_p = 0.0) {
    if (const char *env = std::getenv("IT305_CHECKPOINT_INTERVAL")) {
        try {
            size_t val = std::stoull(env);
            if (val > 0) return val;
        } catch (...) {}
    }
    if (failure_p >= 0.10) {
        return CHUNK_SIZE; // 64 KiB: checkpoint every chunk when p >= 0.1
    } else if (failure_p >= 0.02) {
        return 2 * CHUNK_SIZE; // 128 KiB
    }
    return 1024 * 1024; // 1 MiB default
}

static void serve_client(int fd, fs::path root, double failure_p) {
    std::string line;
    if (!recv_line(fd, line)) {
        close(fd);
        return;
    }

    auto w = split(line);
    if (w.empty()) {
        close(fd);
        return;
    }

    std::string cmd = w[0];
    std::string sid;
    std::string topic;
    std::string resume_file;
    uint64_t resume_offset = 0;
    bool is_resume = false;

    if (cmd == "GET") {
        if (w.size() < 2 || !safe_name(w[1])) {
            send_error(fd, "BAD_REQUEST", "expected_GET_topic");
            close(fd);
            return;
        }
        topic = w[1];
    } else if (cmd == "RESUME") {
        // RESUME <session_id> <topic> <file> <offset>
        if (w.size() < 5) {
            send_error(fd, "BAD_REQUEST", "malformed_RESUME_command");
            close(fd);
            return;
        }
        sid = w[1];
        topic = w[2];
        resume_file = (w[3] == "-" ? "" : w[3]);
        try {
            resume_offset = std::stoull(w[4]);
        } catch (...) {
            send_error(fd, "BAD_OFFSET", "invalid_numeric_offset");
            close(fd);
            return;
        }
        is_resume = true;
    } else {
        send_error(fd, "BAD_REQUEST", "expected_GET_or_RESUME");
        close(fd);
        return;
    }

    fs::path dir = root / topic;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        send_error(fd, "UNKNOWN_TOPIC", topic);
        close(fd);
        return;
    }

    auto all_files = regular_files(dir);
    size_t checkpoint_interval = get_checkpoint_interval(failure_p);

    if (is_resume) {
        std::lock_guard<std::mutex> lk(sessions_mutex);
        auto it = server_sessions.find(sid);
        if (it == server_sessions.end()) {
            send_error(fd, "BAD_SESSION", "unknown_session_id");
            close(fd);
            return;
        }
        if (it->second.topic != topic) {
            send_error(fd, "BAD_SESSION", "topic_mismatch");
            close(fd);
            return;
        }
        if (it->second.status == SessionStatus::COMPLETED) {
            send_error(fd, "BAD_SESSION", "session_already_completed");
            close(fd);
            return;
        }

        // Validate resume file and offset
        if (!resume_file.empty()) {
            fs::path rf_path = dir / resume_file;
            if (!fs::exists(rf_path) || !fs::is_regular_file(rf_path)) {
                send_error(fd, "BAD_FILE", "resume_file_not_found");
                close(fd);
                return;
            }
            std::error_code ec;
            uint64_t actual_size = fs::file_size(rf_path, ec);
            if (resume_offset > actual_size) {
                send_error(fd, "BAD_OFFSET", "resume_offset_exceeds_file_size");
                close(fd);
                return;
            }
            if (resume_offset > it->second.validated_checkpoint_offset &&
                it->second.current_file == resume_file) {
                send_error(fd, "BAD_OFFSET", "resume_offset_exceeds_validated_checkpoint");
                close(fd);
                return;
            }
            it->second.current_file = resume_file;
            it->second.file_size = actual_size;
            it->second.validated_checkpoint_offset = resume_offset;
        }
        it->second.last_active = std::chrono::steady_clock::now();

        // Send RESUME_OK <file> <offset> <checkpoint_interval>
        std::string rf_token = resume_file.empty() ? "-" : resume_file;
        if (!send_line(fd, "RESUME_OK " + rf_token + " " + std::to_string(resume_offset) + " " +
                           std::to_string(checkpoint_interval) + "\n")) {
            close(fd);
            return;
        }
        logline("Session resumed: sid=" + sid + " topic=" + topic +
                " file=" + rf_token + " offset=" + std::to_string(resume_offset));
    } else {
        sid = new_session_id();
        {
            std::lock_guard<std::mutex> lk(sessions_mutex);
            server_sessions[sid] = ServerSession{
                sid,
                topic,
                "",
                0,
                0,
                SessionStatus::ACTIVE,
                {},
                std::chrono::steady_clock::now()
            };
        }
        if (!send_line(fd, "SESSION " + sid + " " + std::to_string(all_files.size()) + " " +
                           std::to_string(checkpoint_interval) + "\n")) {
            close(fd);
            return;
        }
        logline("New session established: sid=" + sid + " topic=" + topic +
                " total_files=" + std::to_string(all_files.size()));
    }

    // Stream files, skipping already completed files (Requirement 8)
    for (const auto &path : all_files) {
        std::string name = path.filename().string();
        uint64_t file_offset = 0;

        {
            std::lock_guard<std::mutex> lk(sessions_mutex);
            auto it = server_sessions.find(sid);
            if (it == server_sessions.end()) {
                close(fd);
                return;
            }
            // Skip completed files
            if (it->second.completed_files.count(name) > 0) {
                continue;
            }
            if (is_resume && !resume_file.empty() && name == resume_file) {
                file_offset = resume_offset;
            }
            it->second.current_file = name;
            std::error_code ec;
            it->second.file_size = fs::file_size(path, ec);
            it->second.validated_checkpoint_offset = file_offset;
            it->second.last_active = std::chrono::steady_clock::now();
        }

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

        if (file_offset > 0) {
            in.seekg(static_cast<std::streamoff>(file_offset));
        }

        // Send FILE <name_length> <file_size> <offset>\n + name bytes
        if (!send_line(fd, "FILE " + std::to_string(name.size()) + " " +
                           std::to_string(size) + " " + std::to_string(file_offset) + "\n") ||
            !send_all(fd, name.data(), name.size())) {
            in.close();
            close(fd);
            return;
        }

        uint64_t pos = file_offset;
        uint64_t bytes_since_last_cp = 0;
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
            bytes_since_last_cp += got;

            // Checkpoint decision: periodic (default 1 MiB) or completed file
            bool need_checkpoint = (bytes_since_last_cp >= checkpoint_interval) || (pos == size);
            if (need_checkpoint) {
                // Wait for client CHECKPOINT <session_id> <file> <offset>\n
                std::string cp_line;
                if (!recv_line(fd, cp_line)) {
                    in.close();
                    close(fd);
                    return;
                }
                auto cp_w = split(cp_line);
                if (cp_w.size() != 4 || cp_w[0] != "CHECKPOINT" ||
                    cp_w[1] != sid || cp_w[2] != name) {
                    send_error(fd, "BAD_CHECKPOINT", "malformed_or_mismatched_checkpoint");
                    in.close();
                    close(fd);
                    return;
                }
                uint64_t cp_offset = 0;
                try {
                    cp_offset = std::stoull(cp_w[3]);
                } catch (...) {
                    send_error(fd, "BAD_CHECKPOINT", "invalid_checkpoint_offset");
                    in.close();
                    close(fd);
                    return;
                }

                if (cp_offset > pos || cp_offset < file_offset) {
                    send_error(fd, "BAD_CHECKPOINT", "offset_out_of_bounds");
                    in.close();
                    close(fd);
                    return;
                }

                // Update server state under mutex
                {
                    std::lock_guard<std::mutex> lk(sessions_mutex);
                    auto it = server_sessions.find(sid);
                    if (it != server_sessions.end()) {
                        it->second.current_file = name;
                        it->second.validated_checkpoint_offset = cp_offset;
                        if (cp_offset == size) {
                            it->second.completed_files.insert(name);
                        }
                        it->second.last_active = std::chrono::steady_clock::now();
                    }
                }

                // Send ACK <session_id> <file> <offset>\n
                if (!send_line(fd, "ACK " + sid + " " + name + " " + std::to_string(cp_offset) + "\n")) {
                    in.close();
                    close(fd);
                    return;
                }
                bytes_since_last_cp = 0;
            }

            // Probabilistic fault injection after chunk transmission
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

        // If file had size 0, send checkpoint & ACK for the completed 0-byte file
        if (size == 0) {
            std::string cp_line;
            if (!recv_line(fd, cp_line)) {
                close(fd);
                return;
            }
            auto cp_w = split(cp_line);
            if (cp_w.size() == 4 && cp_w[0] == "CHECKPOINT" && cp_w[1] == sid && cp_w[2] == name) {
                {
                    std::lock_guard<std::mutex> lk(sessions_mutex);
                    auto it = server_sessions.find(sid);
                    if (it != server_sessions.end()) {
                        it->second.completed_files.insert(name);
                        it->second.validated_checkpoint_offset = 0;
                        it->second.last_active = std::chrono::steady_clock::now();
                    }
                }
                send_line(fd, "ACK " + sid + " " + name + " 0\n");
            }
        }
    }

    // Mark session as completed
    {
        std::lock_guard<std::mutex> lk(sessions_mutex);
        auto it = server_sessions.find(sid);
        if (it != server_sessions.end()) {
            it->second.status = SessionStatus::COMPLETED;
            it->second.last_active = std::chrono::steady_clock::now();
        }
    }

    send_line(fd, "END\n");
    close(fd);
    logline("Transfer complete: sid=" + sid + " topic=" + topic +
            " (" + std::to_string(all_files.size()) + " files)");
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

    logline("Part2_Case2 server listening on port " + std::to_string(port) +
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

        // Multi-threaded server: handle each client connection in an independent thread
        std::thread(serve_client, fd, root, p).detach();
    }
}
