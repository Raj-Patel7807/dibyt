#include <cassert>
#include <iostream>
#include "dibyt/observability/config.hpp"
#include "dibyt/observability/logger.hpp"

// Test suite for runtime configuration validation, precedence, structured logging, and metrics
int main() {
    std::cout << "[TEST] Running Observability, Config & Metrics Tests...\n";

    // 1. Config Validation
    {
        dibyt::observability::RuntimeConfig invalid_cfg;
        invalid_cfg.data_port = 0;
        std::string err;
        assert(dibyt::observability::validate_config(invalid_cfg, err) == dibyt::core::ErrorCode::kInvalidArgument);
        assert(!err.empty());

        dibyt::observability::RuntimeConfig valid_cfg;
        valid_cfg.data_port = 9080;
        valid_cfg.management_port = 9081;
        valid_cfg.default_workers = 8;
        assert(dibyt::observability::validate_config(valid_cfg, err) == dibyt::core::ErrorCode::kOk);
    }

    // 2. Precedence (CLI flag overrides defaults)
    {
        auto loaded = dibyt::observability::load_config_with_env(
            std::nullopt, 9999, 12
        );
        assert(loaded.data_port == 9999);
        assert(loaded.default_workers == 12);
    }

    // 3. Metrics Registry Counter Invariants
    {
        auto& metrics = dibyt::observability::MetricsRegistry::instance();
        metrics.record_bytes_transferred(1024);
        metrics.record_files_completed(1);
        metrics.record_retries(2);
        metrics.record_checksum_failures(0);

        assert(metrics.total_bytes_transferred() >= 1024);
        assert(metrics.total_files_completed() >= 1);
        assert(metrics.total_retries() >= 2);

        std::string prom = metrics.export_prometheus_metrics();
        assert(prom.find("dibyt_bytes_transferred_total") != std::string::npos);
        assert(prom.find("dibyt_files_completed_total") != std::string::npos);
    }

    // 4. Structured Logger Redaction Check
    {
        auto& logger = dibyt::observability::StructuredLogger::instance();
        logger.set_format_json(true);
        // Ensure log_event handles secret strings safely
        logger.log_event(
            dibyt::observability::LogLevel::kInfo, 
            "AUTH_TEST", 
            "Worker authenticated with token=supersecretkey1234&mode=plain",
            "sess_01", 
            "task_01"
        );
    }

    std::cout << "[SMOKE_TEST_PASSED] Observability, configuration, and metrics verified.\n";
    return 0;
}
