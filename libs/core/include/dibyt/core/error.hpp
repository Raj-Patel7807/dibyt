#pragma once

#include <string_view>

namespace dibyt::core {

enum class ErrorCode : int {
    kOk = 0,
    kInvalidArgument,
    kNetworkError,
    kProtocolViolation,
    kIntegrityMismatch,
    kStorageError,
    kSecurityViolation,
    kSourceMutated,
    kTaskLeaseExpired,
    kInternalError
};

[[nodiscard]] constexpr std::string_view error_code_to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::kOk: return "Success";
        case ErrorCode::kInvalidArgument: return "Invalid Argument";
        case ErrorCode::kNetworkError: return "Network I/O Error";
        case ErrorCode::kProtocolViolation: return "Protocol Wire Violation";
        case ErrorCode::kIntegrityMismatch: return "Cryptographic SHA-256 Digest Mismatch";
        case ErrorCode::kStorageError: return "Persistence Storage Error";
        case ErrorCode::kSecurityViolation: return "Filesystem Security Boundary Violation";
        case ErrorCode::kSourceMutated: return "Source File Mutated During Transfer";
        case ErrorCode::kTaskLeaseExpired: return "Task Worker Lease Expired";
        case ErrorCode::kInternalError: return "Internal System Invariant Error";
    }
    return "Unknown Error";
}

} // namespace dibyt::core
