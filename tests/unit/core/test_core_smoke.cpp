#include <cassert>
#include <iostream>
#include <string_view>
#include "dibyt/core/version.hpp"
#include "dibyt/core/error.hpp"

// Smoke test validating version invariants and error mapping
int main() {
    assert(dibyt::core::kProductName == "Dibyt");
    assert(!dibyt::core::kProductVersion.empty());
    assert(dibyt::core::kDefaultChunkSizeBytes == 65536);
    assert(dibyt::core::kDefaultCheckpointIntervalBytes == 1048576);

    assert(dibyt::core::error_code_to_string(dibyt::core::ErrorCode::kOk) == "Success");
    assert(dibyt::core::error_code_to_string(dibyt::core::ErrorCode::kIntegrityMismatch) == "Cryptographic SHA-256 Digest Mismatch");

    std::cout << "[SMOKE_TEST_PASSED] Core foundation invariants verified successfully.\n";
    return 0;
}
