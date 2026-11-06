#include "dibyt/api/router.hpp"
#include "dibyt/core/version.hpp"
#include <sstream>

namespace dibyt::api {

RestApiRouter::RestApiRouter(storage::Database& db, security::AuthManager& auth)
    : db_(db), auth_(auth) {}

std::string RestApiRouter::extract_bearer_token(const HttpRequest& req) const {
    auto it = req.headers.find("Authorization");
    if (it == req.headers.end()) {
        it = req.headers.find("authorization");
    }
    if (it != req.headers.end()) {
        std::string_view auth_val = it->second;
        if (auth_val.starts_with("Bearer ") || auth_val.starts_with("bearer ")) {
            return std::string(auth_val.substr(7));
        }
    }
    return "";
}

HttpResponse RestApiRouter::route_request(const HttpRequest& req) {
    HttpResponse resp;

    // 1. CORS Preflight
    if (req.method == HttpMethod::kOptions) {
        resp.status_code = 204;
        resp.status_text = "No Content";
        return resp;
    }

    // 2. Health & Readiness Endpoint (Public)
    if (req.path == "/health" || req.path == "/healthz") {
        resp.status_code = 200;
        resp.status_text = "OK";
        resp.body = "{\"status\":\"ok\",\"product\":\"" + std::string(core::kProductName) + 
                    "\",\"version\":\"" + std::string(core::kProductVersion) + "\"}";
        return resp;
    }

    // 3. Strict Boundary Enforcement: Reject any bulk binary data POST over REST API
    if (req.path.find("/data") != std::string::npos || req.path.find("/upload") != std::string::npos) {
        resp.status_code = 400;
        resp.status_text = "Bad Request";
        resp.body = "{\"error\":\"ERR_BULK_DATA_DISALLOWED\",\"message\":\"Bulk file bytes must flow over the TCP data plane (port 9080), not HTTP.\"}";
        return resp;
    }

    // 4. Authenticate all /api/v1 routes
    if (req.path.starts_with("/api/v1")) {
        std::string token = extract_bearer_token(req);
        if (!auth_.authenticate_worker(token)) {
            resp.status_code = 401;
            resp.status_text = "Unauthorized";
            resp.body = "{\"error\":\"ERR_UNAUTHORIZED\",\"message\":\"Missing or invalid Bearer authentication token.\"}";
            return resp;
        }

        // Endpoint: /api/v1/status
        if (req.path == "/api/v1/status") {
            auto& metrics = observability::MetricsRegistry::instance();
            std::ostringstream oss;
            oss << "{\"product\":\"" << core::kProductName << "\","
                << "\"version\":\"" << core::kProductVersion << "\","
                << "\"total_bytes_transferred\":" << metrics.total_bytes_transferred() << ","
                << "\"total_files_completed\":" << metrics.total_files_completed() << ","
                << "\"total_retries\":" << metrics.total_retries() << "}";
            resp.status_code = 200;
            resp.body = oss.str();
            return resp;
        }

        // Endpoint: /api/v1/collections
        if (req.path == "/api/v1/collections") {
            resp.status_code = 200;
            resp.body = "{\"collections\":[]}";
            return resp;
        }

        // Endpoint: /api/v1/transfers
        if (req.path == "/api/v1/transfers") {
            resp.status_code = 200;
            resp.body = "{\"transfers\":[]}";
            return resp;
        }
    }

    // 404 Fallback
    resp.status_code = 404;
    resp.status_text = "Not Found";
    resp.body = "{\"error\":\"ERR_NOT_FOUND\",\"message\":\"Unknown endpoint: " + req.path + "\"}";
    return resp;
}

} // namespace dibyt::api
