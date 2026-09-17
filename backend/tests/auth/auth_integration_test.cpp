#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <crow.h>

#include "auth/token_service.hpp"
#include "auth/session_manager.hpp"
#include "db/database.hpp"
#include "config/auth_config.hpp"
#include "middleware/authentication_middleware.hpp"

using namespace auth;
using namespace auth::middleware;

struct AppFixture {
    Database db{":memory:"};
    AuthConfig cfg;
    TokenService token_svc;
    SessionManager session_mgr;

    AppFixture()
        : cfg(make_config())
        , token_svc(cfg)
        , session_mgr(db)
    {
        db.run_migrations("../db/migrations");
    }

    static AuthConfig make_config() {
        AuthConfig c;
        c.jwt_secret = "secret123456789012345678901234567";
        c.jwt_issuer = "test";
        c.jwt_audience = "test";
        c.token_expiry_seconds = 3600;
        return c;
    }
};

TEST_CASE_METHOD(AppFixture, "Middleware strips spoofed headers and applies valid identity", "[integration]") {
    std::string user_id = "test-user-uuid";
    std::string session_id = session_mgr.create_session(user_id, 3600);
    TokenClaims claims{user_id, "t1", session_id, {"user", "admin"}};
    std::string token = token_svc.generate_token(claims);

    AuthenticationMiddleware auth_mid(token_svc, session_mgr);

    crow::request req;
    crow::response res;

    // Simulate malicious client sending spoofed headers
    req.add_header("X-Identity-User-Id", "admin-spoofed");
    req.add_header("X-Identity-Roles", "superadmin");
    req.add_header("Authorization", "Bearer " + token);

    bool auth_success = auth_mid.authenticate(req, res);
    REQUIRE(auth_success == true);

    // Middleware should strip spoofed headers and set legitimate ones
    std::string actual_user_id = req.get_header_value("X-Identity-User-Id");
    REQUIRE(actual_user_id == user_id);

    std::string actual_roles = req.get_header_value("X-Identity-Roles");
    REQUIRE(actual_roles == "user,admin");
}

TEST_CASE_METHOD(AppFixture, "Middleware rejects missing token with standard error", "[integration]") {
    AuthenticationMiddleware auth_mid(token_svc, session_mgr);

    crow::request req;
    crow::response res;

    bool auth_success = auth_mid.authenticate(req, res);
    REQUIRE(auth_success == false);
    REQUIRE(res.code == 401);

    auto body = crow::json::load(res.body);
    REQUIRE(body);
    REQUIRE(body["code"].s() == "UNAUTHENTICATED");
}
