#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
static constexpr size_t CHUNK_SIZE = 64 * 1024;
inline void logline(const std::string &s) {
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lk(log_mutex);
    std::cerr << "[" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "] " << s << "\n";
}
inline bool send_all(int fd, const void *buf, size_t n) {
    const char *p = static_cast<const char*>(buf);
    while (n) {
        ssize_t k = ::send(fd, p, n, MSG_NOSIGNAL);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return false;
        p += k; n -= static_cast<size_t>(k);
    }
    return true;
}
inline bool recv_exact(int fd, void *buf, size_t n) {
    char *p = static_cast<char*>(buf);
    while (n) {
        ssize_t k = ::recv(fd, p, n, 0);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return false;
        p += k; n -= static_cast<size_t>(k);
    }
    return true;
}
inline bool send_line(int fd, const std::string &s) { return send_all(fd, s.data(), s.size()); }
inline bool recv_line(int fd, std::string &out, size_t maxlen=8192) {
    out.clear();
    char c;
    while (out.size() < maxlen) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        if (c == '\n') return true;
        if (c != '\r') out.push_back(c);
    }
    return false;
}
inline std::vector<std::string> split(const std::string &s) {
    std::istringstream is(s);
    std::vector<std::string> v;
    std::string x;
    while (is >> x) {
        v.push_back(x);
    }
    return v;
}
inline bool safe_name(const std::string &s) {
    return !s.empty() && s != "." && s != ".." &&
           s.find('/') == std::string::npos && s.find('\\') == std::string::npos;
}
inline std::vector<fs::path> regular_files(const fs::path &dir) {
    std::vector<fs::path> v;
    std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec))
        if (!ec && e.is_regular_file()) v.push_back(e.path());
    std::sort(v.begin(), v.end());
    return v;
}
inline int connect_to(const std::string &ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1 ||
        connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) < 0) {
        close(fd); return -1;
    }
    return fd;
}
inline int listen_on(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int yes=1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(static_cast<uint16_t>(port)); a.sin_addr.s_addr=htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a))<0 || listen(fd,128)<0) { close(fd); return -1; }
    return fd;
}
inline double now_seconds() {
    using C=std::chrono::steady_clock;
    return std::chrono::duration<double>(C::now().time_since_epoch()).count();
}
