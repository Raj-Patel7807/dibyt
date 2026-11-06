#pragma once

#include <chrono>
#include <random>
#include <cstdint>
#include <string_view>
#include "dibyt/core/error.hpp"

namespace dibyt::transfer {

// Error severity and retryability classification
enum class ErrorClassification {
    kTransientRetryable,  // Network drops, socket timeouts, lease expirations
    kConditionalRetry,    // Cryptographic hash mismatch (up to retry limit)
    kTerminalFatal        // Auth failures, security boundary violations, malformed wire protocol
};

[[nodiscard]] constexpr ErrorClassification classify_error(core::ErrorCode code) noexcept {
    switch (code) {
        case core::ErrorCode::kNetworkError:
        case core::ErrorCode::kTaskLeaseExpired:
            return ErrorClassification::kTransientRetryable;

        case core::ErrorCode::kIntegrityMismatch:
            return ErrorClassification::kConditionalRetry;

        case core::ErrorCode::kInvalidArgument:
        case core::ErrorCode::kProtocolViolation:
        case core::ErrorCode::kSecurityViolation:
        case core::ErrorCode::kSourceMutated:
        case core::ErrorCode::kStorageError:
        case core::ErrorCode::kInternalError:
        case core::ErrorCode::kOk:
            return ErrorClassification::kTerminalFatal;
    }
    return ErrorClassification::kTerminalFatal;
}

struct BackoffPolicy {
    std::chrono::milliseconds initial_delay{100};
    std::chrono::milliseconds max_delay{2000};
    double multiplier{2.0};
    double jitter_factor{0.2}; // +/- 20% randomization to prevent thundering herd
    uint32_t max_retries{5};
};

// Bounded exponential backoff with full jitter calculation
class ExponentialBackoff {
public:
    explicit ExponentialBackoff(BackoffPolicy policy = {})
        : policy_(policy), rng_(std::random_device{}()) {}

    [[nodiscard]] bool can_retry(uint32_t attempt_count, core::ErrorCode error) const noexcept {
        if (classify_error(error) == ErrorClassification::kTerminalFatal) {
            return false;
        }
        return attempt_count < policy_.max_retries;
    }

    [[nodiscard]] std::chrono::milliseconds calculate_delay(uint32_t attempt_count) {
        if (attempt_count == 0) return std::chrono::milliseconds(0);

        // Calculate exponential factor: initial * multiplier^(attempt-1)
        double delay_ms = static_cast<double>(policy_.initial_delay.count());
        for (uint32_t i = 1; i < attempt_count; ++i) {
            delay_ms *= policy_.multiplier;
            if (delay_ms >= static_cast<double>(policy_.max_delay.count())) {
                delay_ms = static_cast<double>(policy_.max_delay.count());
                break;
            }
        }

        // Apply uniform jitter
        std::uniform_real_distribution<double> dist(1.0 - policy_.jitter_factor, 1.0 + policy_.jitter_factor);
        double jittered = delay_ms * dist(rng_);
        return std::chrono::milliseconds(static_cast<int64_t>(jittered));
    }

private:
    BackoffPolicy policy_;
    mutable std::mt19937 rng_;
};

} // namespace dibyt::transfer
