#include "common.hpp"

struct PartitionState {
    std::string sid;
    std::string topic;
    int part = 0;
    int parts = 1;
    std::string current_file;
    uint64_t file_size = 0;
    uint64_t last_committed_offset = 0;
    std::set<std::string> completed_files;
};

static fs::path checkpoint_path(const fs::path &out, const std::string &topic, int part) {
    return out / ("." + topic + ".part" + std::to_string(part) + ".checkpoint");
}

static PartitionState load_checkpoint(const fs::path &cpfile, const std::string &topic, int part, int parts) {
    PartitionState st;
    st.topic = topic;
    st.part = part;
    st.parts = parts;

    std::ifstream f(cpfile);
    if (!f.is_open()) return st;

    std::string tag;
    while (f >> tag) {
        if (tag == "SESSION") {
            f >> st.sid;
        } else if (tag == "TOPIC") {
            f >> st.topic;
        } else if (tag == "PARTITION") {
            f >> st.part >> st.parts;
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

    // If checkpoint is for an incompatible topic or partition parameters, invalidate it
    if (!st.sid.empty() && (st.topic != topic || st.part != part || st.parts != parts)) {
        std::error_code ec;
        fs::remove(cpfile, ec);
        st = PartitionState{};
        st.topic = topic;
        st.part = part;
        st.parts = parts;
    }
    return st;
}

static bool save_checkpoint(const fs::path &cpfile, const PartitionState &st) {
    std::error_code ec;
    fs::path tmp = cpfile.string() + ".tmp";
    std::ofstream f(tmp);
    if (!f.is_open()) return false;

    f << "SESSION " << st.sid << "\n";
    f << "TOPIC " << st.topic << "\n";
    f << "PARTITION " << st.part << " " << st.parts << "\n";
    f << "FILE " << (st.current_file.empty() ? "-" : st.current_file) << " "
      << st.file_size << " " << st.last_committed_offset << "\n";
    f << "COMPLETED " << st.completed_files.size() << "\n";
    for (const auto &fn : st.completed_files) {
        f << fn << "\n";
    }
    f.close();
    fs::rename(tmp, cpfile, ec);
    return !ec;
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
    int max_rec = 75; // Default 75 reconnect attempts per worker
    if (const char *env = std::getenv("IT305_MAX_RECONNECTS")) {
        try {
            int val = std::stoi(env);
            if (val >= 0) max_rec = val;
        } catch (...) {}
    }
    return max_rec;
}

static bool run_partition_worker(const std::string &ip, int port, const std::string &topic,
                                const fs::path &out, int part, int parts) {
    std::error_code ec;
    fs::create_directories(out, ec);
    fs::path target = out / topic;
    fs::create_directories(target, ec);

    fs::path cpfile = checkpoint_path(out, topic, part);
    PartitionState state = load_checkpoint(cpfile, topic, part, parts);

    int max_reconnects = get_max_reconnects();
    int consecutive_reconnect_count = 0;
    int total_reconnects = 0;
    size_t checkpoint_interval = get_checkpoint_interval();

    while (true) {
        bool is_resume = !state.sid.empty();
        if (is_resume) {
            logline("[PART " + std::to_string(part) + "] Resuming session " + state.sid +
                    " (topic=" + topic + " file=" + (state.current_file.empty() ? "(none)" : state.current_file) +
                    " offset=" + std::to_string(state.last_committed_offset) +
                    " completed=" + std::to_string(state.completed_files.size()) + ")");
        } else {
            logline("[PART " + std::to_string(part) + "] Connecting to " + ip + ":" +
                    std::to_string(port) + " for topic=" + topic);
        }

        int fd = connect_to(ip, port);
        if (fd < 0) {
            if (consecutive_reconnect_count >= max_reconnects) {
                logline("Maximum reconnect attempts (" + std::to_string(max_reconnects) +
                        ") reached. Terminating client for part=" + std::to_string(part) + ".");
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            logline("Reconnect attempt " + std::to_string(consecutive_reconnect_count) + "/" +
                    std::to_string(max_reconnects) + " for topic=" + topic +
                    " part=" + std::to_string(part));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Send handshake
        std::string req;
        if (is_resume) {
            std::string rf = state.current_file.empty() ? "-" : state.current_file;
            req = "RESUME " + state.sid + " " + topic + " " + std::to_string(part) + " " +
                  std::to_string(parts) + " " + rf + " " + std::to_string(state.last_committed_offset) + "\n";
        } else {
            req = "GET " + topic + " " + std::to_string(part) + " " + std::to_string(parts) + "\n";
        }

        if (!send_line(fd, req)) {
            close(fd);
            if (consecutive_reconnect_count >= max_reconnects) {
                logline("Maximum reconnect attempts (" + std::to_string(max_reconnects) +
                        ") reached. Terminating client for part=" + std::to_string(part) + ".");
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            logline("Reconnect attempt " + std::to_string(consecutive_reconnect_count) + "/" +
                    std::to_string(max_reconnects) + " for topic=" + topic +
                    " part=" + std::to_string(part));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::string line;
        if (!recv_line(fd, line)) {
            close(fd);
            if (consecutive_reconnect_count >= max_reconnects) {
                logline("Maximum reconnect attempts (" + std::to_string(max_reconnects) +
                        ") reached. Terminating client for part=" + std::to_string(part) + ".");
                return false;
            }
            consecutive_reconnect_count++;
            total_reconnects++;
            logline("Reconnect attempt " + std::to_string(consecutive_reconnect_count) + "/" +
                    std::to_string(max_reconnects) + " for topic=" + topic +
                    " part=" + std::to_string(part));
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
                // Invalidate stale session for this partition only
                logline("[PART " + std::to_string(part) + "] Stale/invalid session reported: " + line +
                        ". Discarding partition checkpoint and establishing fresh session...");
                fs::remove(cpfile, ec);
                state = PartitionState{};
                state.topic = topic;
                state.part = part;
                state.parts = parts;
                close(fd);
                continue;
            } else {
                logline("[PART " + std::to_string(part) + "] Fatal server error: " + line);
                close(fd);
                return false;
            }
        }

        if (is_resume) {
            if (w[0] != "RESUME_OK") {
                logline("[PART " + std::to_string(part) + "] Protocol error: expected RESUME_OK, got: " + line);
                close(fd);
                return false;
            }
            if (w.size() >= 4) {
                try {
                    size_t srv_interval = std::stoull(w[3]);
                    if (srv_interval > 0) checkpoint_interval = srv_interval;
                } catch (...) {}
            }
            logline("[PART " + std::to_string(part) + "] Resume confirmed: " + line);
        } else {
            if (w[0] != "SESSION" || w.size() < 3) {
                logline("[PART " + std::to_string(part) + "] Protocol error: expected SESSION, got: " + line);
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
            logline("[PART " + std::to_string(part) + "] Session established: sid=" + state.sid +
                    " files=" + w[2]);
        }

        // Send START control message
        if (!send_line(fd, "START\n")) {
            close(fd);
            continue;
        }

        bool stream_error = false;
        bool transfer_completed = false;

        // File stream loop
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
                logline("[PART " + std::to_string(part) + "] Server error: " + line);
                close(fd);
                return false;
            }
            if (w[0] != "FILE" || w.size() != 4) {
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
                mode |= std::ios::in;
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
                logline("[PART " + std::to_string(part) + "] Failed to open file: " + dest.string());
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
                    logline("[PART " + std::to_string(part) + "] Write error for file: " + name);
                    stream_error = true;
                    break;
                }

                pos += chunk_bytes;
                bytes_since_last_cp += chunk_bytes;

                // Checkpoint check: 1 MiB interval or file EOF
                bool need_checkpoint = (bytes_since_last_cp >= checkpoint_interval) || (pos == file_size);
                if (need_checkpoint) {
                    std::string cp_msg = "CHECKPOINT " + state.sid + " " + name + " " + std::to_string(pos) + "\n";
                    if (!send_line(fd, cp_msg)) {
                        stream_error = true;
                        break;
                    }

                    std::string ack_line;
                    if (!recv_line(fd, ack_line)) {
                        stream_error = true;
                        break;
                    }
                    auto ack_w = split(ack_line);
                    if (ack_w.size() != 4 || ack_w[0] != "ACK" ||
                        ack_w[1] != state.sid || ack_w[2] != name ||
                        ack_w[3] != std::to_string(pos)) {
                        logline("[PART " + std::to_string(part) + "] Invalid checkpoint ACK: " + ack_line);
                        stream_error = true;
                        break;
                    }

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

            // Handle 0-byte file
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
            // Delete this partition's checkpoint upon full completion
            fs::remove(cpfile, ec);
            logline("[PART " + std::to_string(part) + "] Partition transfer completed successfully!");
            return true;
        }

        // Connection interrupted
        if (consecutive_reconnect_count >= max_reconnects) {
            logline("Maximum reconnect attempts (" + std::to_string(max_reconnects) +
                    ") reached. Terminating client for part=" + std::to_string(part) + ".");
            return false;
        }

        consecutive_reconnect_count++;
        total_reconnects++;
        logline("Reconnect attempt " + std::to_string(consecutive_reconnect_count) + "/" +
                std::to_string(max_reconnects) + " for topic=" + topic +
                " part=" + std::to_string(part));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char **argv) {
    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <topic> <output_dir> [parallel_partitions]\n";
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

    // Enhanced mode uses 3 parallel partitions by default (configurable via 5th arg)
    int parts = 3;
    if (argc == 6) {
        try {
            int val = std::stoi(argv[5]);
            if (val >= 1 && val <= 32) parts = val;
            else {
                std::cerr << "Error: parallel_partitions must be in range [1, 32].\n";
                return 2;
            }
        } catch (...) {
            std::cerr << "Error: invalid parallel_partitions parameter.\n";
            return 2;
        }
    }

    double start_time = now_seconds();
    logline("Starting Enhanced transfer for topic '" + topic + "' with " +
            std::to_string(parts) + " parallel connection(s)...");

    // Spawn concurrent worker threads for true parallel socket transfers
    std::vector<std::thread> workers;
    std::vector<bool> worker_results(parts, false);

    for (int i = 0; i < parts; ++i) {
        workers.emplace_back([&, i]() {
            worker_results[i] = run_partition_worker(ip, port, topic, out, i, parts);
        });
    }

    for (auto &t : workers) {
        if (t.joinable()) t.join();
    }

    bool all_ok = true;
    for (int i = 0; i < parts; ++i) {
        if (!worker_results[i]) {
            all_ok = false;
            logline("Partition " + std::to_string(i) + " failed.");
        }
    }

    double elapsed = std::max(0.000001, now_seconds() - start_time);
    if (!all_ok) {
        std::cerr << "Transfer failed: one or more partitions failed to complete.\n";
        return 1;
    }

    uint64_t total_useful_bytes = 0;
    size_t file_count = 0;
    std::error_code ec;
    fs::path target = out / topic;
    for (const auto &p : fs::directory_iterator(target, ec)) {
        if (p.is_regular_file()) {
            total_useful_bytes += p.file_size();
            file_count++;
        }
    }

    std::cout << "TRANSFER_OK topic=" << topic << " parts=" << parts
              << " useful_bytes=" << total_useful_bytes
              << " elapsed_seconds=" << elapsed
              << " throughput_Bps=" << (total_useful_bytes / elapsed)
              << " files=" << file_count << "\n";

    logline("All " + std::to_string(parts) + " partitions completed successfully! Total files: " +
            std::to_string(file_count) + " (" + std::to_string(total_useful_bytes) + " bytes in " +
            std::to_string(elapsed) + "s)");

    return 0;
}
