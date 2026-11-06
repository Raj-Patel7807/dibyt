#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>
#include "dibyt/core/error.hpp"
#include "dibyt/security/auth.hpp"
#include "dibyt/storage/database.hpp"
#include "dibyt/observability/logger.hpp"

namespace dibyt::api {

enum class HttpMethod {
    kGet,
    kPost,
    kPut,
    kDelete,
    kOptions,
    kUnsupported
};

struct HttpRequest {
    HttpMethod method{HttpMethod::kUnsupported};
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code{200};
    std::string status_text{"OK"};
    std::unordered_map<std::string, std::string> headers{
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Headers", "Authorization, Content-Type"}
    };
    std::string body;
};

// Dispatcher for versioned REST API (/api/v1)
class RestApiRouter {
public:
    explicit RestApiRouter(storage::Database& db, security::AuthManager& auth);

    // Route an incoming HTTP request to the appropriate controller handler
    [[nodiscard]] HttpResponse route_request(const HttpRequest& req);

private:
    [[nodiscard]] std::string extract_bearer_token(const HttpRequest& req) const;

    storage::Database& db_;
    security::AuthManager& auth_;
};

} // namespace dibyt::api
