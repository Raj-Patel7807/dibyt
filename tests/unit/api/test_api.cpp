#include <cassert>
#include <iostream>
#include "dibyt/api/router.hpp"

// Test suite for management REST API routing, authentication, and plane boundaries
int main() {
    std::cout << "[TEST] Running Versioned Management REST API Tests...\n";

    std::filesystem::path temp_db = std::filesystem::temp_directory_path() / "dibyt_api_test.db";
    std::filesystem::remove(temp_db);

    dibyt::storage::Database db;
    assert(db.open(temp_db) == dibyt::core::ErrorCode::kOk);

    auto& auth = dibyt::security::AuthManager::instance();
    std::string valid_token = "admin_bearer_token_xyz";
    auth.register_valid_token(valid_token);

    dibyt::api::RestApiRouter router(db, auth);

    // 1. Health Endpoint (Public, No Token)
    {
        dibyt::api::HttpRequest req;
        req.method = dibyt::api::HttpMethod::kGet;
        req.path = "/health";

        auto resp = router.route_request(req);
        assert(resp.status_code == 200);
        assert(resp.body.find("\"status\":\"ok\"") != std::string::npos);
    }

    // 2. Unauthenticated /api/v1/status Request -> 401 Unauthorized
    {
        dibyt::api::HttpRequest req;
        req.method = dibyt::api::HttpMethod::kGet;
        req.path = "/api/v1/status";

        auto resp = router.route_request(req);
        assert(resp.status_code == 401);
        assert(resp.body.find("ERR_UNAUTHORIZED") != std::string::npos);
    }

    // 3. Authenticated /api/v1/status Request -> 200 OK
    {
        dibyt::api::HttpRequest req;
        req.method = dibyt::api::HttpMethod::kGet;
        req.path = "/api/v1/status";
        req.headers["Authorization"] = "Bearer " + valid_token;

        auto resp = router.route_request(req);
        assert(resp.status_code == 200);
        assert(resp.body.find("\"product\":\"Dibyt\"") != std::string::npos);
        assert(resp.body.find("total_bytes_transferred") != std::string::npos);
    }

    // 4. Strict Plane Separation: Disallow Bulk Payload via REST API -> 400 Bad Request
    {
        dibyt::api::HttpRequest req;
        req.method = dibyt::api::HttpMethod::kPost;
        req.path = "/api/v1/upload/data";
        req.headers["Authorization"] = "Bearer " + valid_token;

        auto resp = router.route_request(req);
        assert(resp.status_code == 400);
        assert(resp.body.find("ERR_BULK_DATA_DISALLOWED") != std::string::npos);
    }

    // 5. CORS Preflight -> 204 No Content
    {
        dibyt::api::HttpRequest req;
        req.method = dibyt::api::HttpMethod::kOptions;
        req.path = "/api/v1/status";

        auto resp = router.route_request(req);
        assert(resp.status_code == 204);
        assert(resp.headers.find("Access-Control-Allow-Origin") != resp.headers.end());
    }

    auth.revoke_token(valid_token);
    db.close();
    std::filesystem::remove(temp_db);

    std::cout << "[SMOKE_TEST_PASSED] Versioned REST API routing and security boundaries verified.\n";
    return 0;
}
