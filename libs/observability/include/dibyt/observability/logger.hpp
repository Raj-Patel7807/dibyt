#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <atomic>
#include <mutex>

namespace dibyt::observability {

// Structured log levels
enum class LogLevel {
    kDebug,
    kInfo,
    kWarn,
    kError
};

// Structured JSON event logger with automatic secret redaction
class StructuredLogger {
public:
    static StructuredLogger& instance();

    void log_event(
        LogLevel level,
        std::string_view event_name,
        std::string_view message,
        std::string_view session_id = "",
        std::string_view task_id = ""
    );

    void set_format_json(bool json) noexcept { json_format_ = json; }

private:
    StructuredLogger() = default;
    std::mutex mutex_;
    bool json_format_{false};
};

// Operational metrics counters and gauges
class MetricsRegistry {
public:
    static MetricsRegistry& instance();

    void record_bytes_transferred(uint64_t bytes) noexcept {
        total_bytes_transferred_.fetch_add(bytes, std::memory_order_relaxed);
    }

    void record_files_completed(uint32_t count = 1) noexcept {
        total_files_completed_.fetch_add(count, std::memory_order_relaxed);
    }

    void record_retries(uint32_t count = 1) noexcept {
        total_retries_.fetch_add(count, std::memory_order_relaxed);
    }

    void record_checksum_failures(uint32_t count = 1) noexcept {
        total_checksum_failures_.fetch_add(count, std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t total_bytes_transferred() const noexcept {
        return total_bytes_transferred_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint32_t total_files_completed() const noexcept {
        return total_files_completed_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint32_t total_retries() const noexcept {
        return total_retries_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] uint32_t total_checksum_failures() const noexcept {
        return total_checksum_failures_.load(std::memory_order_relaxed);
    }

    // Export Prometheus exposition format
    [[nodiscard]] std::string export_prometheus_metrics() const;

private:
    MetricsRegistry() = default;
    std::atomic<uint64_t> total_bytes_transferred_{0};
    std::atomic<uint32_t> total_files_completed_{0};
    std::atomic<uint32_t> total_retries_{0};
    std::atomic<uint32_t> total_checksum_failures_{0};
};

} // namespace dibyt::observability
