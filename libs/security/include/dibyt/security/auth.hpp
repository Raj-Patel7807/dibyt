#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <mutex>
#include <cstdint>
#include <filesystem>
#include "dibyt/core/error.hpp"

namespace dibyt::security {

// Constant-time string comparison to defend against timing side-channel attacks
[[nodiscard]] bool constant_time_equals(std::string_view a, std::string_view b) noexcept;

// Session and Worker Authentication Engine
class AuthManager {
public:
    static AuthManager& instance();

    void register_valid_token(std::string_view token);
    void revoke_token(std::string_view token);

    // Default deny: verifies presented token in constant time
    [[nodiscard]] bool authenticate_worker(std::string_view token) const;

    // Checks permission for operations: "publish", "fetch", "admin"
    [[nodiscard]] bool authorize_action(
        std::string_view token, 
        std::string_view action
    ) const;

private:
    AuthManager() = default;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> valid_tokens_;
};

// Filesystem Jail Sandbox Enforcer
class FilesystemJail {
public:
    explicit FilesystemJail(std::filesystem::path root_jail_path);

    // Validates that relative path resolves strictly inside jail root, rejecting symlink and '..' escapes
    [[nodiscard]] core::ErrorCode resolve_safe_path(
        std::string_view relative_path,
        std::filesystem::path& out_safe_path
    ) const;

    [[nodiscard]] const std::filesystem::path& jail_root() const noexcept { return jail_root_; }

private:
    std::filesystem::path jail_root_;
};

} // namespace dibyt::security
