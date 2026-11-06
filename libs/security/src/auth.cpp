#include "dibyt/security/auth.hpp"
#include "dibyt/manifest/manifest.hpp"

namespace dibyt::security {

bool constant_time_equals(std::string_view a, std::string_view b) noexcept {
    if (a.length() != b.length()) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < a.length(); ++i) {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

AuthManager& AuthManager::instance() {
    static AuthManager auth;
    return auth;
}

void AuthManager::register_valid_token(std::string_view token) {
    std::lock_guard<std::mutex> lock(mutex_);
    valid_tokens_.insert(std::string(token));
}

void AuthManager::revoke_token(std::string_view token) {
    std::lock_guard<std::mutex> lock(mutex_);
    valid_tokens_.erase(std::string(token));
}

bool AuthManager::authenticate_worker(std::string_view token) const {
    if (token.empty()) {
        return false; // Default deny
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& valid : valid_tokens_) {
        if (constant_time_equals(token, valid)) {
            return true;
        }
    }
    return false; // Default deny
}

bool AuthManager::authorize_action(std::string_view token, std::string_view action) const {
    if (!authenticate_worker(token)) {
        return false; // Unauthenticated -> unauthorized
    }
    if (action.empty()) {
        return false;
    }
    // Granular action policy: valid token authorizes fetch and publish
    if (action == "fetch" || action == "publish" || action == "status") {
        return true;
    }
    return false;
}

FilesystemJail::FilesystemJail(std::filesystem::path root_jail_path)
    : jail_root_(std::filesystem::weakly_canonical(std::move(root_jail_path))) {}

core::ErrorCode FilesystemJail::resolve_safe_path(
    std::string_view relative_path,
    std::filesystem::path& out_safe_path
) const {
    std::string sanitized;
    auto err = manifest::sanitize_relative_path(std::filesystem::path(relative_path), sanitized);
    if (err != core::ErrorCode::kOk) {
        return core::ErrorCode::kSecurityViolation;
    }

    std::filesystem::path candidate = jail_root_ / std::filesystem::path(sanitized);
    std::filesystem::path canonical_candidate = std::filesystem::weakly_canonical(candidate);

    // Verify canonical prefix starts with canonical jail root
    std::string jail_str = jail_root_.string();
    std::string cand_str = canonical_candidate.string();

    if (cand_str.rfind(jail_str, 0) != 0) {
        return core::ErrorCode::kSecurityViolation; // Directory traversal escape detected
    }

    out_safe_path = std::move(canonical_candidate);
    return core::ErrorCode::kOk;
}

} // namespace dibyt::security
