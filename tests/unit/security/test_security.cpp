#include <cassert>
#include <iostream>
#include <filesystem>
#include "dibyt/security/auth.hpp"

// Test suite for token authentication, constant-time comparison, and filesystem jail sandboxing
int main() {
    std::cout << "[TEST] Running Security Foundations & Filesystem Jail Tests...\n";

    // 1. Constant-Time String Comparison
    assert(dibyt::security::constant_time_equals("token_secret_123", "token_secret_123"));
    assert(!dibyt::security::constant_time_equals("token_secret_123", "token_secret_124"));
    assert(!dibyt::security::constant_time_equals("short", "longer_token"));

    // 2. AuthManager Default-Deny & Token Lifecycle
    auto& auth = dibyt::security::AuthManager::instance();
    std::string test_tok = "sec_auth_token_alpha";

    // Unregistered token must be denied
    assert(!auth.authenticate_worker(test_tok));
    assert(!auth.authorize_action(test_tok, "fetch"));

    // Register token
    auth.register_valid_token(test_tok);
    assert(auth.authenticate_worker(test_tok));
    assert(auth.authorize_action(test_tok, "fetch"));
    assert(auth.authorize_action(test_tok, "publish"));
    assert(!auth.authorize_action(test_tok, "unsupported_superadmin"));

    // Revoke token
    auth.revoke_token(test_tok);
    assert(!auth.authenticate_worker(test_tok));

    // 3. Filesystem Jail Sandboxing & Traversal Rejection
    std::filesystem::path jail_dir = std::filesystem::temp_directory_path() / "dibyt_sec_jail";
    std::filesystem::remove_all(jail_dir);
    std::filesystem::create_directories(jail_dir / "safe_dir");

    dibyt::security::FilesystemJail jail(jail_dir);

    // Safe path within boundary
    std::filesystem::path resolved;
    assert(jail.resolve_safe_path("safe_dir/file.txt", resolved) == dibyt::core::ErrorCode::kOk);
    assert(resolved.string().find(jail_dir.string()) == 0);

    // Traversal payload escaping jail boundary: MUST BE REJECTED
    assert(jail.resolve_safe_path("../../Windows/System32/cmd.exe", resolved) == dibyt::core::ErrorCode::kSecurityViolation);
    assert(jail.resolve_safe_path("../secret.key", resolved) == dibyt::core::ErrorCode::kSecurityViolation);

    std::filesystem::remove_all(jail_dir);

    std::cout << "[SMOKE_TEST_PASSED] Security authentication and filesystem jail defenses verified.\n";
    return 0;
}
