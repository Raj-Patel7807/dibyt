#include "common.hpp"

struct ClientState {
    std::string sid;
    std::string topic;
    std::string current_file;
    uint64_t file_size = 0;
    uint64_t last_committed_offset = 0;
    std::set<std::string> completed_files;
};

static fs::path checkpoint_path(const fs::path &out, const std::string &topic) {
    return out / ("." + topic + ".checkpoint");
}

static ClientState load_checkpoint(const fs::path &cpfile) {
    ClientState st;
    std::ifstream f(cpfile);
    if (!f.is_open()) return st;

    std::string tag;
    while (f >> tag) {
        if (tag == "SESSION") {
            f >> st.sid;
        } else if (tag == "TOPIC") {
            f >> st.topic;
        } else if (tag == "FILE") {
            f >> st.current_file >> st.file_size >> st.last_committed_offset;
            if (st.current_file == "-") st.current_file.clear();
        } else if (tag == "COMPLETED") {
            size_t count = 0;
            if (f >> count) {
                for (size_t i = 0; i < count; ++i) {
                    std::string fn;
                    if (f >> fn) st.completed_files.insert(fn);
                }
            }
        }
    }
    return st;
}

static void save_checkpoint(const fs::path &cpfile, const ClientState &st) {
    std::error_code ec;
    fs::path tmp = cpfile.string() + ".tmp";
    std::ofstream f(tmp);
    if (!f.is_open()) return;

    f << "SESSION " << st.sid << "\n";
    f << "TOPIC " << st.topic << "\n";
    f << "FILE " << (st.current_file.empty() ? "-" : st.current_file) << " "
      << st.file_size << " " << st.last_committed_offset << "\n";
    f << "COMPLETED " << st.completed_files.size() << "\n";
    for (const auto &fn : st.completed_files) {
        f << fn << "\n";
    }
    f.close();
    fs::rename(tmp, cpfile, ec);
}

static size_t get_checkpoint_interval() {
    size_t interval = 1024 * 1024; // 1 MiB default
    if (const char *env = std::getenv("IT305_CHECKPOINT_INTERVAL")) {
        try {
            size_t val = std::stoull(env);
            if (val > 0) interval = val;
        } catch (...) {}
    }
    return interval;
}

static int get_max_reconnects() {
    int max_rec = 75; // Default 75 reconnect attempts
    if (const char *env = std::getenv("IT305_MAX_RECONNECTS")) {
        try {
            int val = std::stoi(env);
            if (val >= 0) max_rec = val;
        } catch (...) {}
    }
    return max_rec;
}

static bool run_case2_client(const std::string &ip, int port, const std::string &topic, const fs::path &out) {
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::path target = out / topic;
    fs::create_directories(target, ec);

    fs::path cpfile = checkpoint_path(out, topic);
    ClientState state = load_checkpoint(cpfile);
    if (!state.sid.empty() && state.topic != topic) {
        // Discard checkpoint if it was for a different topic
        fs::remove(cpfile, ec);
        state = ClientState{};
    }
    state.topic = topic;

    int max_reconnects = get_max_reconnects();
    int consecutive_reconnect_count = 0;
    int total_reconnects = 0;
    size_t checkpoint_interval = get_checkpoint_interval();
    double overall_start = now_seconds();

    while (true) {
        bool is_resume = !state.sid.empty();
        if (is_resume) {
            std::cerr << "[CLIENT] Connecting to " << ip << ":" << port
                      << " to resume session " << state.sid << " (topic: " << topic
                      << ", file: " << (state.current_file.empty() ? "(none)" : state.current_file)
                      << ", offset: " << state.last_committed_offset
                      << ", completed files: " << state.completed_files.size() << ")...\n";
        } else {
            std::cerr << "[CLIENT] Connecting to " << ip << ":" << port
                      << " for topic '" << topic << "'...\n";
        }

        int fd = connect_to(ip, port);
        if (fd < 0) {
            if (consecutive_reconnect_count >= max_reconnects) {
                std::cerr << "Maximum reconnect attempts (" << max_reconnects
                          << ") reached. Terminating client.\n";
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            std::cerr << "[CLIENT] Reconnect attempt " << consecutive_reconnect_count << "/" << max_reconnects
                      << ": Server connection failed. Retrying in 100ms...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Handshake: send RESUME if we have session state, otherwise send GET
        std::string req;
        if (is_resume) {
            std::string rf = state.current_file.empty() ? "-" : state.current_file;
            req = "RESUME " + state.sid + " " + topic + " " + rf + " " +
                  std::to_string(state.last_committed_offset) + "\n";
        } else {
            req = "GET " + topic + "\n";
        }

        if (!send_line(fd, req)) {
            close(fd);
            if (consecutive_reconnect_count >= max_reconnects) {
                std::cerr << "Maximum reconnect attempts (" << max_reconnects
                          << ") reached. Terminating client.\n";
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            std::cerr << "[CLIENT] Reconnect attempt " << consecutive_reconnect_count << "/" << max_reconnects
                      << ": Failed to send handshake. Retrying...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::string line;
        if (!recv_line(fd, line)) {
            close(fd);
            if (consecutive_reconnect_count >= max_reconnects) {
                std::cerr << "Maximum reconnect attempts (" << max_reconnects
                          << ") reached. Terminating client.\n";
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            std::cerr << "[CLIENT] Reconnect attempt " << consecutive_reconnect_count << "/" << max_reconnects
                      << ": Connection closed before handshake response. Retrying...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto w = split(line);
        if (w.empty()) {
            close(fd);
            continue;
        }

        if (w[0] == "ERROR") {
            if (w.size() >= 2 && w[1] == "BAD_SESSION") {
                // Stale or expired session on server: reset checkpoint and start fresh
                std::cerr << "[CLIENT] Warning: Server reported stale/invalid session (" << line
                          << "). Resetting session checkpoint and restarting from beginning...\n";
                fs::remove(cpfile, ec);
                state = ClientState{};
                state.topic = topic;
                close(fd);
                continue;
            } else {
                std::cerr << "[CLIENT] Fatal server error: " << line << "\n";
                close(fd);
                return false;
            }
        }

        if (is_resume) {
            if (w[0] != "RESUME_OK") {
                std::cerr << "[CLIENT] Protocol error: expected RESUME_OK, got: " << line << "\n";
                close(fd);
                return false;
            }
            if (w.size() >= 4) {
                try {
                    size_t srv_interval = std::stoull(w[3]);
                    if (srv_interval > 0) checkpoint_interval = srv_interval;
                } catch (...) {}
            }
            std::cerr << "[CLIENT] Server confirmed resume: " << line << "\n";
        } else {
            if (w[0] != "SESSION" || w.size() < 3) {
                std::cerr << "[CLIENT] Protocol error: expected SESSION, got: " << line << "\n";
                close(fd);
                return false;
            }
            state.sid = w[1];
            if (w.size() >= 4) {
                try {
                    size_t srv_interval = std::stoull(w[3]);
                    if (srv_interval > 0) checkpoint_interval = srv_interval;
                } catch (...) {}
            }
            save_checkpoint(cpfile, state);
            std::cerr << "[CLIENT] New session established: sid=" << state.sid
                      << ", total files: " << w[2] << "\n";
        }

        bool stream_error = false;
        bool transfer_completed = false;

        // File stream reception loop
        while (recv_line(fd, line)) {
            w = split(line);
            if (w.empty()) {
                stream_error = true;
                break;
            }
            if (w[0] == "END") {
                transfer_completed = true;
                break;
            }
            if (w[0] == "ERROR") {
                std::cerr << "[CLIENT] Server reported error: " << line << "\n";
                close(fd);
                return false;
            }
            if (w[0] != "FILE" || w.size() != 4) {
                std::cerr << "[CLIENT] Unexpected line header: " << line << "\n";
                stream_error = true;
                break;
            }

            size_t nlen = 0;
            uint64_t file_size = 0, file_offset = 0;
            try {
                nlen = std::stoull(w[1]);
                file_size = std::stoull(w[2]);
                file_offset = std::stoull(w[3]);
            } catch (...) {
                stream_error = true;
                break;
            }

            if (nlen == 0 || nlen > 4096 || file_offset > file_size) {
                stream_error = true;
                break;
            }

            std::string name(nlen, '\0');
            if (!recv_exact(fd, name.data(), nlen) || !safe_name(name)) {
                stream_error = true;
                break;
            }

            state.current_file = name;
            state.file_size = file_size;
            state.last_committed_offset = file_offset;
            save_checkpoint(cpfile, state);

            fs::path dest = target / name;
            std::ios::openmode mode = std::ios::binary | std::ios::out;
            if (file_offset > 0) {
                mode |= std::ios::in; // Open existing file for random-access resume
            } else {
                mode |= std::ios::trunc;
            }

            std::fstream file(dest, mode);
            if (!file.is_open()) {
                std::ofstream create(dest, std::ios::binary);
                create.close();
                file.open(dest, std::ios::binary | std::ios::in | std::ios::out);
            }
            if (!file.is_open()) {
                std::cerr << "[CLIENT] Failed to open local file: " << dest.string() << "\n";
                stream_error = true;
                break;
            }

            file.seekp(static_cast<std::streamoff>(file_offset));
            uint64_t pos = file_offset;
            uint64_t bytes_since_last_cp = 0;

            while (pos < file_size) {
                if (!recv_line(fd, line)) {
                    stream_error = true;
                    break;
                }
                w = split(line);
                if (w.size() != 3 || w[0] != "DATA") {
                    stream_error = true;
                    break;
                }

                uint64_t chunk_off = 0;
                size_t chunk_bytes = 0;
                try {
                    chunk_off = std::stoull(w[1]);
                    chunk_bytes = std::stoull(w[2]);
                } catch (...) {
                    stream_error = true;
                    break;
                }

                if (chunk_off != pos || chunk_bytes == 0 ||
                    chunk_bytes > CHUNK_SIZE || pos + chunk_bytes > file_size) {
                    stream_error = true;
                    break;
                }

                std::vector<char> buf(chunk_bytes);
                if (!recv_exact(fd, buf.data(), chunk_bytes)) {
                    stream_error = true;
                    break;
                }

                file.write(buf.data(), static_cast<std::streamsize>(chunk_bytes));
                file.flush();
                if (!file.good()) {
                    std::cerr << "[CLIENT] File write error for " << name << "\n";
                    stream_error = true;
                    break;
                }

                pos += chunk_bytes;
                bytes_since_last_cp += chunk_bytes;

                // Checkpoint check: periodic (default 1 MiB) or file completion
                bool need_checkpoint = (bytes_since_last_cp >= checkpoint_interval) || (pos == file_size);
                if (need_checkpoint) {
                    // Send CHECKPOINT <session_id> <file> <offset>\n
                    std::string cp_msg = "CHECKPOINT " + state.sid + " " + name + " " + std::to_string(pos) + "\n";
                    if (!send_line(fd, cp_msg)) {
                        stream_error = true;
                        break;
                    }

                    // Await ACK <session_id> <file> <offset>\n
                    std::string ack_line;
                    if (!recv_line(fd, ack_line)) {
                        stream_error = true;
                        break;
                    }
                    auto ack_w = split(ack_line);
                    if (ack_w.size() != 4 || ack_w[0] != "ACK" ||
                        ack_w[1] != state.sid || ack_w[2] != name ||
                        ack_w[3] != std::to_string(pos)) {
                        std::cerr << "[CLIENT] Invalid checkpoint ACK: " << ack_line << "\n";
                        stream_error = true;
                        break;
                    }

                    // ACK received: commit checkpoint to disk
                    state.last_committed_offset = pos;
                    if (pos == file_size) {
                        state.completed_files.insert(name);
                    }
                    save_checkpoint(cpfile, state);
                    bytes_since_last_cp = 0;
                    consecutive_reconnect_count = 0; // Forward progress made
                }
            }

            file.close();
            if (stream_error) break;

            // Handle 0-byte file checkpoint
            if (file_size == 0) {
                std::string cp_msg = "CHECKPOINT " + state.sid + " " + name + " 0\n";
                if (send_line(fd, cp_msg)) {
                    std::string ack_line;
                    if (recv_line(fd, ack_line)) {
                        auto ack_w = split(ack_line);
                        if (ack_w.size() == 4 && ack_w[0] == "ACK") {
                            state.completed_files.insert(name);
                            state.last_committed_offset = 0;
                            save_checkpoint(cpfile, state);
                            consecutive_reconnect_count = 0; // Forward progress made
                        }
                    }
                }
            }
        }

        close(fd);

        if (transfer_completed) {
            // Remove checkpoint file only upon successful completion
            fs::remove(cpfile, ec);
            double elapsed = std::max(0.000001, now_seconds() - overall_start);
            uint64_t useful_bytes = 0;
            size_t count = 0;
            for (const auto &p : fs::directory_iterator(target, ec)) {
                if (p.is_regular_file()) {
                    useful_bytes += p.file_size();
                    count++;
                }
            }
            std::cerr << "[CLIENT] Success: " << count << " files (" << useful_bytes
                      << " bytes) transferred in " << (total_reconnects + 1) << " connection(s) over "
                      << elapsed << "s ("
                      << (static_cast<double>(useful_bytes) / (1024.0 * 1024.0 * elapsed)) << " MB/s)\n";

            std::cout << "TRANSFER_OK topic=" << topic << " part=0 useful_bytes=" << useful_bytes
                      << " elapsed_seconds=" << elapsed
                      << " throughput_Bps=" << (useful_bytes / elapsed)
                      << " reconnects=" << total_reconnects << "\n";
            return true;
        }

        // Connection was interrupted mid-stream
        if (consecutive_reconnect_count >= max_reconnects) {
            std::cerr << "Maximum reconnect attempts (" << max_reconnects
                      << ") reached. Terminating client.\n";
            // Preserve checkpoint and partial files on disk!
            return false;
        }

        consecutive_reconnect_count++;
        total_reconnects++;
        std::cerr << "[CLIENT] Reconnect attempt " << consecutive_reconnect_count << "/" << max_reconnects
                  << ": Connection interrupted during transfer of '" << state.current_file
                  << "' (last committed checkpoint: " << state.last_committed_offset
                  << " bytes). Reconnecting in 100ms...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
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
    return run_case2_client(ip, port, topic, out) ? 0 : 1;
}
