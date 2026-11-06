#include "dibyt/observability/logger.hpp"
#include <iostream>
#include <chrono>
#include <sstream>

namespace dibyt::observability {

namespace {

std::string get_iso8601_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_buf);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

std::string redact_secrets(std::string_view msg) {
    std::string s(msg);
    // Basic redactor replacing sensitive tokens
    const std::string token_prefix = "token=";
    size_t pos = s.find(token_prefix);
    if (pos != std::string::npos) {
        size_t end = s.find_first_of(" &,\"\n", pos + token_prefix.length());
        s.replace(pos + token_prefix.length(), 
                  (end == std::string::npos ? s.length() : end) - (pos + token_prefix.length()), 
                  "***REDACTED***");
    }
    return s;
}

const char* level_to_str(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo: return "INFO";
        case LogLevel::kWarn: return "WARN";
        case LogLevel::kError: return "ERROR";
    }
    return "INFO";
}

} // namespace

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger logger;
    return logger;
}

void StructuredLogger::log_event(
    LogLevel level,
    std::string_view event_name,
    std::string_view message,
    std::string_view session_id,
    std::string_view task_id
) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string safe_msg = redact_secrets(message);
    std::string ts = get_iso8601_timestamp();

    if (json_format_) {
        std::cout << "{\"timestamp\":\"" << ts << "\","
                  << "\"level\":\"" << level_to_str(level) << "\","
                  << "\"event\":\"" << event_name << "\","
                  << "\"message\":\"" << safe_msg << "\"";
        if (!session_id.empty()) std::cout << ",\"session_id\":\"" << session_id << "\"";
        if (!task_id.empty()) std::cout << ",\"task_id\":\"" << task_id << "\"";
        std::cout << "}\n";
    } else {
        std::cout << "[" << ts << "] [" << level_to_str(level) << "] "
                  << event_name << ": " << safe_msg;
        if (!session_id.empty()) std::cout << " [session=" << session_id << "]";
        if (!task_id.empty()) std::cout << " [task=" << task_id << "]";
        std::cout << "\n";
    }
}

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry reg;
    return reg;
}

std::string MetricsRegistry::export_prometheus_metrics() const {
    std::ostringstream oss;
    oss << "# HELP dibyt_bytes_transferred_total Total raw data bytes transferred\n"
        << "# TYPE dibyt_bytes_transferred_total counter\n"
        << "dibyt_bytes_transferred_total " << total_bytes_transferred() << "\n\n"
        << "# HELP dibyt_files_completed_total Total files successfully received and verified\n"
        << "# TYPE dibyt_files_completed_total counter\n"
        << "dibyt_files_completed_total " << total_files_completed() << "\n\n"
        << "# HELP dibyt_retries_total Total retry attempts executed\n"
        << "# TYPE dibyt_retries_total counter\n"
        << "dibyt_retries_total " << total_retries() << "\n\n"
        << "# HELP dibyt_checksum_failures_total Total cryptographic integrity verification failures\n"
        << "# TYPE dibyt_checksum_failures_total counter\n"
        << "dibyt_checksum_failures_total " << total_checksum_failures() << "\n";
    return oss.str();
}

} // namespace dibyt::observability
