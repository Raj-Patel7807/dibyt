#include <cassert>
#include <iostream>
#include "dibyt/transfer/recovery.hpp"

// Test suite for error retry classification and bounded exponential backoff
int main() {
    std::cout << "[TEST] Running Fault Tolerance, Retry & Recovery Tests...\n";

    // 1. Error Classification Matrix
    assert(dibyt::transfer::classify_error(dibyt::core::ErrorCode::kNetworkError) == 
           dibyt::transfer::ErrorClassification::kTransientRetryable);
    assert(dibyt::transfer::classify_error(dibyt::core::ErrorCode::kTaskLeaseExpired) == 
           dibyt::transfer::ErrorClassification::kTransientRetryable);
    assert(dibyt::transfer::classify_error(dibyt::core::ErrorCode::kIntegrityMismatch) == 
           dibyt::transfer::ErrorClassification::kConditionalRetry);

    // Terminal violations must NEVER be retried
    assert(dibyt::transfer::classify_error(dibyt::core::ErrorCode::kSecurityViolation) == 
           dibyt::transfer::ErrorClassification::kTerminalFatal);
    assert(dibyt::transfer::classify_error(dibyt::core::ErrorCode::kProtocolViolation) == 
           dibyt::transfer::ErrorClassification::kTerminalFatal);

    // 2. Exponential Backoff with Jitter Policy
    dibyt::transfer::BackoffPolicy policy;
    policy.initial_delay = std::chrono::milliseconds(50);
    policy.max_delay = std::chrono::milliseconds(500);
    policy.max_retries = 3;

    dibyt::transfer::ExponentialBackoff backoff(policy);

    // Terminal error must not be retried
    assert(!backoff.can_retry(1, dibyt::core::ErrorCode::kSecurityViolation));
    assert(!backoff.can_retry(1, dibyt::core::ErrorCode::kProtocolViolation));

    // Transient network error can be retried up to max_retries
    assert(backoff.can_retry(1, dibyt::core::ErrorCode::kNetworkError));
    assert(backoff.can_retry(2, dibyt::core::ErrorCode::kNetworkError));
    assert(!backoff.can_retry(3, dibyt::core::ErrorCode::kNetworkError)); // Retry ceiling hit

    // 3. Jittered Delay Bounds
    for (uint32_t attempt = 1; attempt <= 3; ++attempt) {
        auto delay = backoff.calculate_delay(attempt);
        assert(delay.count() > 0);
        assert(delay.count() <= 600); // Bounded within max delay + jitter margin
    }

    std::cout << "[SMOKE_TEST_PASSED] Error classification and bounded backoff verified successfully.\n";
    return 0;
}
